/*
 * Copyright (C) 2016 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define G_LOG_DOMAIN "gssdp-net"

#include <config.h>

#include "gssdp-net.h"
#include "gssdp-error.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <ifaddrs.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

gboolean
gssdp_net_init (GError **error)
{
        return TRUE;
}

void
gssdp_net_shutdown (void)
{
}

int
gssdp_net_query_ifindex (GSSDPNetworkDevice *device)
{
#if defined(HAVE_IFNAMETOINDEX)
        errno = 0;
        int index = if_nametoindex (device->iface_name);
        if (index == 0 && errno != 0) {
                return -1;
        } else {
                return index;
        }

#elif defined(HAVE_SIOCGIFINDEX)
    int fd;
    int result;
    struct ifreq ifr;

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    memset (&ifr, 0, sizeof(struct ifreq));
    strncpy (ifr.ifr_ifrn.ifrn_name, device->iface_name, IFNAMSIZ);

    result = ioctl (fd, SIOCGIFINDEX, (char *)&ifr);
    close (fd);

    if (result == 0)
        return ifr.ifr_ifindex;
    else
        return -1;
#else
    return -1;
#endif
}

#if defined(__linux__)
struct nl_req_s {
    struct nlmsghdr hdr;
    struct ndmsg gen;
};

#define NLMSG_IS_VALID(msg,len) \
        (NLMSG_OK(msg,len) && (msg->nlmsg_type != NLMSG_DONE))

#define RT_ATTR_OK(a,l) \
        ((l > 0) && RTA_OK (a, l))

char *
gssdp_net_mac_lookup (GSSDPNetworkDevice *device, const char *ip_address)
{
        int fd = -1;
        int saved_errno;
        int status;
        struct sockaddr_nl sa, dest;
        struct nl_req_s req;
        char *result = NULL;
        int seq = rand();
        GInetAddress *addr = NULL;
        struct iovec iov;
        struct msghdr msg;
        char buf[8196];
        unsigned char *data = NULL;
        gssize data_length = -1;

        /* Create the netlink socket */
        fd = socket (PF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK, NETLINK_ROUTE);
        saved_errno = errno;

        if (fd == -1) {
                g_debug ("Failed to create netlink socket: %s",
                         g_strerror (saved_errno));
                goto out;
        }

        memset (&sa, 0, sizeof (sa));
        sa.nl_family = AF_NETLINK;
        status = bind (fd, (struct sockaddr *) &sa, sizeof (sa));
        saved_errno = errno;
        if (status == -1) {
                g_debug ("Failed ot bind to netlink socket: %s",
                         g_strerror (saved_errno));

                goto out;
        }

        /* Query the current neighbour table */
        memset (&req, 0, sizeof (req));
        memset (&dest, 0, sizeof (dest));
        memset (&msg, 0, sizeof (msg));

        dest.nl_family = AF_NETLINK;
        req.hdr.nlmsg_len = NLMSG_LENGTH (sizeof (struct ndmsg));
        req.hdr.nlmsg_seq = seq;
        req.hdr.nlmsg_type = RTM_GETNEIGH;
        req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

        addr = g_inet_address_new_from_string (ip_address);
        req.gen.ndm_family = g_inet_address_get_family (addr);

        iov.iov_base = &req;
        iov.iov_len = req.hdr.nlmsg_len;

        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_name = &dest;
        msg.msg_namelen = sizeof (dest);

        status = sendmsg (fd, (struct msghdr *) &msg, 0);
        saved_errno = errno;

        if (status < 0) {
                g_debug ("Failed to send netlink message: %s",
                         g_strerror (saved_errno));

                goto out;
        }

        /* Receive the answers until error or nothing more to read */
        while (TRUE) {
                ssize_t len;
                struct nlmsghdr *header = (struct nlmsghdr *) buf;

                len = recv (fd, buf, sizeof (buf), 0);
                saved_errno = errno;
                if (len < 0) {
                        if (saved_errno != EWOULDBLOCK && saved_errno != EAGAIN) {
                                g_debug ("Failed to receive netlink msg: %s",
                                         g_strerror (saved_errno));
                        }

                        break;
                }

                for (; NLMSG_IS_VALID (header, len); header = NLMSG_NEXT (header, len)) {
                        struct ndmsg *msg;
                        struct rtattr *rtattr;
                        int rtattr_len;

                        if (header->nlmsg_type != RTM_NEWNEIGH)
                                continue;

                        msg = NLMSG_DATA (header);

                        rtattr = RTM_RTA (msg);
                        rtattr_len = RTM_PAYLOAD (header);

                        while (RT_ATTR_OK (rtattr, rtattr_len)) {
                                if (rtattr->rta_type == NDA_DST) {
                                        GInetAddress *entry_addr = g_inet_address_new_from_bytes (RTA_DATA (rtattr),
                                                        g_inet_address_get_family (addr));
                                        gboolean equal = g_inet_address_equal (addr, entry_addr);
                                        g_clear_object (&entry_addr);

                                        if (!equal) {
                                                g_clear_pointer (&data, g_free);
                                                break;
                                        }
                                } else if (rtattr->rta_type == NDA_LLADDR) {
                                        g_clear_pointer (&data, g_free);
                                        data_length = RTA_PAYLOAD (rtattr);
#if GLIB_CHECK_VERSION(2, 68, 0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                                        data = g_memdup2 (RTA_DATA (rtattr), data_length);
#pragma GCC diagnostic pop
#else
                                        data = g_memdup (RTA_DATA (rtattr), data_length);
#endif
                                }

                                rtattr = RTA_NEXT (rtattr, rtattr_len);
                        }

                        if (data != NULL)
                                break;
                }

                if (data != NULL)
                        break;

        }

        if (data != NULL) {
                gssize i;
                GString *mac_str = g_string_new ("");
                for (i = 0; i < data_length; i++) {
                        if (i > 0) {
                                g_string_append_c (mac_str, ':');
                        }
                        g_string_append_printf (mac_str, "%02x", data[i]);
                }

                result = g_string_free (mac_str, FALSE);
        }
out:
        g_clear_pointer (&data, g_free);
        g_clear_object (&addr);
        if (fd >= 0)
                close (fd);

        if (result == NULL)
                return g_strdup (ip_address);
        else
                return result;
}
#else
char *
gssdp_net_mac_lookup (GSSDPNetworkDevice *device, const char *ip_address)
{
        return g_strdup (ip_address);
}
#endif

static const char *
sockaddr_to_string(struct sockaddr *addr,
                   gchar           *result_buf,
                   gsize            result_buf_len)
{
    char *buf = NULL;
    const char *retval = NULL;
    sa_family_t family = addr->sa_family;
    g_return_val_if_fail (family == AF_INET || family == AF_INET6, NULL);

    if (family == AF_INET) {
           struct sockaddr_in *sa = (struct sockaddr_in *) addr;
            buf = (char *)&sa->sin_addr;
    } else {
            struct sockaddr_in6 *sa = (struct sockaddr_in6 *) addr;
            buf = (char *)&sa->sin6_addr;
    }

    retval = inet_ntop (family, buf, result_buf, result_buf_len);
    if (retval == NULL) {
            g_warning ("Failed to convert address: %s", g_strerror (errno));
    }

    return retval;
}

static GInetAddress *
get_host_addr (struct sockaddr *addr)
{
        guint8 *buf = NULL;
        sa_family_t family = addr->sa_family;
        g_return_val_if_fail (family == AF_INET || family == AF_INET6, NULL);

        if (family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *) addr;
                buf = (guint8 *)&sa->sin_addr;
        } else {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *) addr;
                buf = (guint8 *)&sa->sin6_addr;
        }

        return g_inet_address_new_from_bytes (buf, family);
}

#define HI(x) (((x) & 0xf0) >> 4)
#define LO(x) (((x) & 0x0f))
static GInetAddressMask *
get_netmask (struct sockaddr *address,
             struct sockaddr *mask)
{
        static const gint8 bits_map[] = {
                 0, -1, -1, -1,
                -1, -1, -1, -1,
                 1, -1, -1, -1,
                 2, -1,  3,  4
        };

        const guint8 *addr_buf = NULL;
        const guint8 *mask_buf = NULL;
        int buflen = 0;
        guint8 prefix[16] = { 0 };
        int i = 0;
        gboolean done = FALSE;
        int bits = 0;
        GInetAddress *result_address = NULL;
        GInetAddressMask *result = NULL;
        GError *error = NULL;

        g_return_val_if_fail (address != NULL, NULL);
        g_return_val_if_fail (address->sa_family == mask->sa_family, NULL);
        g_return_val_if_fail (address->sa_family == G_SOCKET_FAMILY_IPV4 ||
                              address->sa_family == G_SOCKET_FAMILY_IPV6, NULL);
        g_return_val_if_fail (mask != NULL, NULL);

        if (address->sa_family == G_SOCKET_FAMILY_IPV4) {
                struct sockaddr_in *s4  = (struct sockaddr_in *) address;
                addr_buf = (const guint8 *) &(s4->sin_addr);
                s4 = (struct sockaddr_in *) mask;
                mask_buf = (const guint8 *) &(s4->sin_addr);
                buflen = 4;
        } else if (address->sa_family == G_SOCKET_FAMILY_IPV6) {
                struct sockaddr_in6 *s6  = (struct sockaddr_in6 *) address;
                addr_buf = (const guint8 *) &(s6->sin6_addr);
                s6 = (struct sockaddr_in6 *) mask;
                mask_buf = (const guint8 *) &(s6->sin6_addr);
                buflen = 16;
        } else
                g_assert_not_reached ();

        for (i = 0; i < buflen; i++) {
                /* Invalid netmask with holes in it */
                if (done && mask_buf[i] != 0x00) {
                        return NULL;
                }

                prefix[i] = addr_buf[i] & mask_buf[i];

                if (mask_buf[i] == 0xff)
                        bits += 8;
                else {
                        done = TRUE;
                        /* if the upper nibble isn't all bits set, the lower nibble must be 0 */
                        if (HI(mask_buf[i]) != 0x0f && LO(mask_buf[i]) != 0x00) {
                                return NULL;
                        }

                        /* Only valid bit patterns have correct values set in bits_map
                         * -1 means fail so the mask is invalid  */
                        if (bits_map[HI(mask_buf[i])] == -1) {
                                return NULL;
                        }

                        if (bits_map[LO(mask_buf[i])] == -1) {
                                return NULL;
                        }

                        bits += bits_map[HI(mask_buf[i])] + bits_map[LO(mask_buf[i])];
                }
        }

        result_address = g_inet_address_new_from_bytes (prefix, address->sa_family);
        result = g_inet_address_mask_new (result_address, bits, &error);
        g_clear_object (&result_address);

        if (result == NULL) {
                g_warning ("Failed to create netmask: %s", error->message);
                g_clear_error (&error);
        }

        return result;
}



gboolean
gssdp_net_get_host_ip (GSSDPNetworkDevice *device, GError **error)
{
        struct ifaddrs *ifa_list, *ifa;
        GList *up_ifaces, *ifaceptr;
        char addr_string[INET6_ADDRSTRLEN] = {0};
        sa_family_t family = AF_UNSPEC;

        up_ifaces = NULL;

        errno = 0;
        if (getifaddrs (&ifa_list) != 0) {
                int saved_errno = errno;
                g_set_error (
                        error,
                        G_IO_ERROR,
                        g_io_error_from_errno (saved_errno),
                        "Failed to retrieve list of network interfaces: %s",
                        g_strerror (errno));

                return FALSE;
        }

        /*
         * First, check all the devices. Filter out everything that is not UP or
         * a PtP device or matches a supported family (FIXME: Questionable; it might
         * be useful to do SSDP on a PtP device, though)
         */
        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                /* Can happen for weird sa_family */
                if (ifa->ifa_addr == NULL) {
                        continue;
                }

                /* We are really interested in AF_INET* only */
                family = ifa->ifa_addr->sa_family;
                if (family != AF_INET && family != AF_INET6) {
                        continue;
                }

                else if (device->iface_name &&
                    !g_str_equal (device->iface_name, ifa->ifa_name)) {
                        continue;
                }

                else if (!(ifa->ifa_flags & IFF_UP))
                        continue;

                else if ((ifa->ifa_flags & IFF_POINTOPOINT))
                        continue;

                /* Loopback and legacy IP interfaces go at the bottom on the list */
                if ((ifa->ifa_flags & IFF_LOOPBACK) || family == AF_INET6) {
                        g_debug ("Found %s(%s), appending",
                                 ifa->ifa_name,
                                 sockaddr_to_string (ifa->ifa_addr,
                                                     addr_string,
                                                     sizeof (addr_string)));
                        up_ifaces = g_list_append (up_ifaces, ifa);
                } else {
                        g_debug ("Found %s(%s), prepending",
                                 ifa->ifa_name,
                                 sockaddr_to_string (ifa->ifa_addr,
                                                     addr_string,
                                                     sizeof (addr_string)));
                        up_ifaces = g_list_prepend (up_ifaces, ifa);
                }
        }

        /*
         * Now go through the devices we consider worthy
         */
        family = device->address_family;

        /* If we have an address, its family will take precendence.
         * Otherwise take the family from the client's config
         */
        if (device->host_addr) {
                family = g_inet_address_get_family (device->host_addr);
                // Address was solely added to select the address family
                if (g_inet_address_get_is_any (device->host_addr))
                        g_clear_object (&device->host_addr);
        }

        for (ifaceptr = up_ifaces;
             ifaceptr != NULL;
             ifaceptr = ifaceptr->next) {
                GInetAddress *device_addr = NULL;
                gboolean equal = FALSE;

                ifa = ifaceptr->data;

                /* There was an address given for the client, but
                 * the address families don't match -> skip
                 */
                if (family != G_SOCKET_FAMILY_INVALID &&
                    ifa->ifa_addr->sa_family != family) {
                        continue;
                }

                device_addr = get_host_addr (ifa->ifa_addr);

                if (device->host_addr == NULL) {
                        switch (ifa->ifa_addr->sa_family) {
                        case AF_INET:
                        case AF_INET6:
                                device->host_addr = g_object_ref (device_addr);
                                break;
                        default:
                                /* We filtered this out in the list before */
                                g_assert_not_reached ();
                        }
                }

                equal = g_inet_address_equal (device_addr, device->host_addr);
                g_clear_object (&device_addr);

                /* There was an host address set but it does not match the current address */
                if (!equal)
                        continue;

                if (device->host_mask != NULL &&
                    !g_inet_address_mask_matches (device->host_mask,
                                                  device->host_addr)) {
                        g_clear_object (&device->host_mask);
                }

                if (device->host_mask == NULL) {
                        device->host_mask =
                                get_netmask (ifa->ifa_addr, ifa->ifa_netmask);
                }

                if (device->iface_name == NULL)
                        device->iface_name = g_strdup (ifa->ifa_name);
                else {
                        // We have found the address, and we have an iface. Does it match?
                        if (!g_str_equal (device->iface_name, ifa->ifa_name)) {
                                g_set_error(error, GSSDP_ERROR, GSSDP_ERROR_FAILED, "Information mismatch: Interface passed address is %s, but requested %s",
                                             device->iface_name, ifa->ifa_name);
                                return FALSE;
                        }
                }

                if (device->network == NULL)
                        device->network = g_inet_address_mask_to_string (device->host_mask);

                g_clear_pointer (&device->host_ip, g_free);
                device->host_ip = g_inet_address_to_string (device->host_addr);

                device->index = gssdp_net_query_ifindex (device);

                break;
        }

        if (device->host_addr != NULL) {
                device->address_family = g_inet_address_get_family (device->host_addr);
        }

        g_list_free (up_ifaces);
        freeifaddrs (ifa_list);

        return TRUE;
}

GList *
gssdp_net_list_devices (void)
{
        struct ifaddrs *ifa_list, *ifa;
        GHashTable *interfaces;
        GList *result = NULL;

        interfaces = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

        if (getifaddrs (&ifa_list) != 0) {
                g_warning ("Failed to retrieve list of network interfaces: %s",
                           strerror (errno));

                goto out;
        }

        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                g_hash_table_add (interfaces, g_strdup (ifa->ifa_name));
        }


        freeifaddrs (ifa_list);

out:
        result = g_hash_table_get_keys (interfaces);
        g_hash_table_steal_all (interfaces);
        g_hash_table_destroy (interfaces);

        return result;
}
