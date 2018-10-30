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

#ifdef __linux__
#include <net/if_arp.h>
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

char *
gssdp_net_arp_lookup (GSSDPNetworkDevice *device, const char *ip_address)
{
#if defined(__linux__)
        struct arpreq req;
        struct sockaddr_in *sin;
        int fd = -1;

        memset (&req, 0, sizeof (req));

        /* FIXME: Update when we support IPv6 properly */
        sin = (struct sockaddr_in *) &req.arp_pa;
        sin->sin_family = AF_INET;
        sin->sin_addr.s_addr = inet_addr (ip_address);

        /* copy name, leave place for nul terminator */;
        strncpy (req.arp_dev, device->iface_name, sizeof (req.arp_dev) - 1);

        fd = socket (AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
                return g_strdup (ip_address);

        if (ioctl (fd, SIOCGARP, (caddr_t) &req) < 0) {
                return NULL;
        }
        close (fd);

        if (req.arp_flags & ATF_COM) {
                unsigned char *buf = (unsigned char *) req.arp_ha.sa_data;

                return g_strdup_printf ("%02X:%02X:%02X:%02X:%02X:%02X",
                                        buf[0],
                                        buf[1],
                                        buf[2],
                                        buf[3],
                                        buf[4],
                                        buf[5]);
        }

        return g_strdup (ip_address);
#else
        return g_strdup (ip_address);
#endif
}

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
        g_warning ("Failed to convert address: %s", g_strerror(errno));
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
