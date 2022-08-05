/*
 * Copyright (C) 2016 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>

#include "gssdp-net.h"

#include <glib.h>

#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#ifdef __linux__
#include <net/if_arp.h>
#endif

#include <android/log.h>

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
gssdp_net_mac_lookup (GSSDPNetworkDevice *device, const char *ip_address)
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

gboolean
gssdp_net_get_host_ip (GSSDPNetworkDevice *device, GError **error)
{
        struct      ifreq *ifaces = NULL;
        struct      ifreq *iface = NULL;
        struct      ifreq tmp_iface;
        struct      ifconf ifconfigs;
        struct      sockaddr_in *address, *netmask;
        struct      in_addr net_address;
        uint32_t    ip;
        int         if_buf_size, sock, i, if_num;
        GList       *if_ptr, *if_list = NULL;

        if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
                __android_log_write (ANDROID_LOG_WARN,
                                     "gssdp",
                                     "Couldn't create socket");
                return FALSE;
        }

        /* Fill ifaces with the available interfaces
         * we incrementally proceed in chunks of 4
         * till getting the full list
         */

        if_buf_size = 0;
        do {
                if_buf_size += 4 * sizeof (struct ifreq);
                ifaces = g_realloc (ifaces, if_buf_size);
                ifconfigs.ifc_len = if_buf_size;
                ifconfigs.ifc_buf = (char *) ifaces;

                /* FIXME: IPv4 only. This ioctl only deals with AF_INET */
                if (ioctl (sock, SIOCGIFCONF, &ifconfigs) == -1) {
                        __android_log_print (ANDROID_LOG_WARN, "gssdp",
                                "Couldn't get list of devices. Asked for: %d",
                                if_buf_size / sizeof (struct ifreq));

                        goto fail;
                }

        } while (ifconfigs.ifc_len >= if_buf_size);

        if_num = ifconfigs.ifc_len / sizeof (struct ifreq);

        if (!device->iface_name) {
                __android_log_print (ANDROID_LOG_DEBUG, "gssdp",
                        "Got list of %d interfaces. Looking for a suitable one",
                        if_num);
        } else {
                __android_log_print (ANDROID_LOG_DEBUG, "gssdp",
                        "List of %d interfaces ready. Now finding %s",
                        if_num, device->iface_name);
        }

        /* Buildup prioritized interface list
         */

        for (i = 0; i < if_num; i++) {

                address = (struct sockaddr_in *) &(ifaces[i].ifr_addr);

                __android_log_print (ANDROID_LOG_DEBUG,
                                     "gssdp",
                                     "Trying interface: %s",
                                     ifaces[i].ifr_name);

                if (!address->sin_addr.s_addr) {
                        __android_log_write (ANDROID_LOG_DEBUG, "gssdp",
                                "No configured address. Discarding");
                        continue;
                }

                memcpy (&tmp_iface, &ifaces[i], sizeof (struct ifreq));

                if (ioctl (sock, SIOCGIFFLAGS, &tmp_iface) == -1) {
                        __android_log_write (ANDROID_LOG_DEBUG, "gssdp",
                                "Couldn't get flags. Discarding");
                        continue;
                }

                /* If an specific interface query was passed over.. */
                if (device->iface_name &&
                    g_strcmp0 (device->iface_name, tmp_iface.ifr_name)) {
                        continue;
                } else if (!(tmp_iface.ifr_flags & IFF_UP) ||
                           tmp_iface.ifr_flags & IFF_POINTOPOINT) {
                        continue;
                }

                /* Prefer non loopback */
                if (ifaces[i].ifr_flags & IFF_LOOPBACK)
                        if_list = g_list_append (if_list, ifaces + i);
                else
                        if_list = g_list_prepend (if_list, ifaces + i);

                if (device->iface_name)
                    break;
        }

        if (!g_list_length (if_list)) {
                __android_log_write (ANDROID_LOG_DEBUG,
                                     "gssdp",
                                     "No usable interfaces found");
                goto fail;
        }

        /* Fill device with data from the first interface
         * we can get complete config info for and return
         */

        for (if_ptr = if_list; if_ptr != NULL;
             if_ptr = g_list_next (if_ptr)) {

                iface   = (struct ifreq *) if_ptr->data;
                address = (struct sockaddr_in *) &(iface->ifr_addr);
                netmask = (struct sockaddr_in *) &(iface->ifr_netmask);

                device->host_ip = g_malloc0 (INET_ADDRSTRLEN);

                if (inet_ntop (AF_INET, &(address->sin_addr),
                        device->host_ip, INET_ADDRSTRLEN) == NULL) {

                        __android_log_print (ANDROID_LOG_INFO,
                                             "gssdp",
                                             "Failed to get ip for: %s, %s",
                                             iface->ifr_name,
                                             strerror (errno));

                        g_free (device->host_ip);
                        device->host_ip = NULL;
                        continue;
                }
                device->host_addr = g_inet_address_new_from_string (device->host_ip);

                ip = address->sin_addr.s_addr;

                if (ioctl (sock, SIOCGIFNETMASK, iface) == -1) {
                        __android_log_write (ANDROID_LOG_DEBUG, "gssdp",
                                "Couldn't get netmask. Discarding");
                        g_free (device->host_ip);
                        device->host_ip = NULL;
                        continue;
                }

                memcpy (&device->mask, netmask, sizeof (struct sockaddr_in));

                if (device->network == NULL) {
                        device->network = g_malloc0 (INET_ADDRSTRLEN);

                        net_address.s_addr = ip & netmask->sin_addr.s_addr;

                        if (inet_ntop (AF_INET, &net_address,
                            device->network, INET_ADDRSTRLEN) == NULL) {

                                __android_log_print (ANDROID_LOG_WARN, "gssdp",
                                        "Failed to get nw for: %s, %s",
                                        iface->ifr_name, strerror (errno));

                                g_clear_pointer (&device->host_ip, g_free);
                                g_clear_pointer (&device->network, g_free);
                                g_clear_object (&device->host_addr);
                                continue;
                        }
                }

                if (!device->iface_name)
                    device->iface_name = g_strdup (iface->ifr_name);

                goto success;

        }

        __android_log_write (ANDROID_LOG_WARN, "gssdp",
                "Traversed whole list without finding a configured device");

fail:
        __android_log_write (ANDROID_LOG_WARN,
                             "gssdp",
                             "Failed to get configuration for device");
        g_free (ifaces);
        g_list_free (if_list);
        close (sock);
        return FALSE;
success:
        __android_log_print (ANDROID_LOG_DEBUG, "gssdp",
                "Returned config params for device: %s ip: %s network: %s",
                device->iface_name, device->host_ip, device->network);
        g_free (ifaces);
        g_list_free (if_list);
        close (sock);
        return TRUE;
}

GList *
gssdp_net_list_devices (void)
{
        return NULL;
}
