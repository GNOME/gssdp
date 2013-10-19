
#include <config.h>
#include <sys/types.h>
#include <glib.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdlib.h>

#include "gssdp-net.h"

gboolean
gssdp_net_init (GError **error)
{
        return TRUE;
}

void
gssdp_net_shutdown (void)
{
        /* Do nothing. */
}

gboolean
gssdp_net_get_host_ip (GSSDPNetworkDevice *device)
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

                                g_free (device->host_ip);
                                device->host_ip = NULL;
                                g_free (device->network);
                                device->network = NULL;
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
