/*
 * Copyright (C) 2016 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#define G_LOG_DOMAIN "gssdp-net"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "gssdp-net.h"

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
    return if_nametoindex (device->iface_name);

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

                        rtattr = IFA_RTA (msg);
                        rtattr_len = IFA_PAYLOAD (header);

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
                                        data = g_memdup (RTA_DATA (rtattr), data_length);
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

gboolean
gssdp_net_get_host_ip (GSSDPNetworkDevice *device)
{
        struct ifaddrs *ifa_list, *ifa;
        GList *up_ifaces, *ifaceptr;
        char addr_string[INET6_ADDRSTRLEN] = {0};
        sa_family_t family = AF_UNSPEC;

        up_ifaces = NULL;

        if (getifaddrs (&ifa_list) != 0) {
                g_warning ("Failed to retrieve list of network interfaces: %s",
                           strerror (errno));

                return FALSE;
        }

        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL)
                        continue;

                family = ifa->ifa_addr->sa_family;
                if (family != AF_INET && family != AF_INET6) {
                    continue;
                }

                if (device->iface_name &&
                    !g_str_equal (device->iface_name, ifa->ifa_name)) {
                        g_debug ("Skipping %s because it does not match %s",
                                 ifa->ifa_name,
                                 device->iface_name);
                        continue;
                } else if (!(ifa->ifa_flags & IFF_UP)) {
                        g_debug ("Skipping %s because it is not up",
                                 ifa->ifa_name);
                        continue;
                } else if ((ifa->ifa_flags & IFF_POINTOPOINT)) {
                        g_debug ("Skipping %s because it is point-to-point",
                                 ifa->ifa_name);
                        continue;
                }

                /* Loopback and IPv6 interfaces go at the bottom on the list */

                if ((ifa->ifa_flags & IFF_LOOPBACK) ||
                    family == AF_INET6) {
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

        for (ifaceptr = up_ifaces;
             ifaceptr != NULL;
             ifaceptr = ifaceptr->next) {
                char ip[INET6_ADDRSTRLEN];
                char net[INET6_ADDRSTRLEN];
                const char *p, *q;
                struct sockaddr_in *s4, *s4_mask;
                struct in_addr net_addr;
                const guint8 *bytes;

                ifa = ifaceptr->data;

                if (ifa->ifa_addr->sa_family != AF_INET) {
                        continue;
                }

                s4 = (struct sockaddr_in *) ifa->ifa_addr;
                p = inet_ntop (AF_INET, &s4->sin_addr, ip, sizeof (ip));
                device->host_ip = g_strdup (p);

                bytes = (const guint8 *) &s4->sin_addr;
                device->host_addr = g_inet_address_new_from_bytes
                                        (bytes, G_SOCKET_FAMILY_IPV4);

                s4_mask = (struct sockaddr_in *) ifa->ifa_netmask;
                memcpy (&(device->mask), s4_mask, sizeof (struct sockaddr_in));
                net_addr.s_addr = (in_addr_t) s4->sin_addr.s_addr &
                                  (in_addr_t) s4_mask->sin_addr.s_addr;
                q = inet_ntop (AF_INET, &net_addr, net, sizeof (net));


                if (device->iface_name == NULL)
                        device->iface_name = g_strdup (ifa->ifa_name);
                if (device->network == NULL)
                        device->network = g_strdup (q);

                device->index = gssdp_net_query_ifindex (device);
                break;
        }

        g_list_free (up_ifaces);
        freeifaddrs (ifa_list);

        return TRUE;
}
