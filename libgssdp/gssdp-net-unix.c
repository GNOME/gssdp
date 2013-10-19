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
#include <ifaddrs.h>

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
        struct ifaddrs *ifa_list, *ifa;
        GList *up_ifaces, *ifaceptr;

        up_ifaces = NULL;

        if (getifaddrs (&ifa_list) != 0) {
                g_error ("Failed to retrieve list of network interfaces:\n%s\n",
                         strerror (errno));

                return FALSE;
        }

        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL)
                        continue;

                if (device->iface_name &&
                    strcmp (device->iface_name, ifa->ifa_name) != 0)
                        continue;
                else if (!(ifa->ifa_flags & IFF_UP))
                        continue;
                else if ((ifa->ifa_flags & IFF_POINTOPOINT))
                        continue;

                /* Loopback and IPv6 interfaces go at the bottom on the list */
                if ((ifa->ifa_flags & IFF_LOOPBACK) ||
                    ifa->ifa_addr->sa_family == AF_INET6)
                        up_ifaces = g_list_append (up_ifaces, ifa);
                else
                        up_ifaces = g_list_prepend (up_ifaces, ifa);
        }

        for (ifaceptr = up_ifaces;
             ifaceptr != NULL;
             ifaceptr = ifaceptr->next) {
                char ip[INET6_ADDRSTRLEN];
                char net[INET6_ADDRSTRLEN];
                const char *p, *q;
                struct sockaddr_in *s4, *s4_mask;
                struct in_addr net_addr;

                ifa = ifaceptr->data;

                if (ifa->ifa_addr->sa_family != AF_INET) {
                        continue;
                }

                s4 = (struct sockaddr_in *) ifa->ifa_addr;
                p = inet_ntop (AF_INET,
                               &s4->sin_addr,
                               ip,
                               sizeof (ip));
                device->host_ip = g_strdup (p);
                s4_mask = (struct sockaddr_in *) ifa->ifa_netmask;
                memcpy (&(device->mask), s4_mask, sizeof (struct sockaddr_in));
                net_addr.s_addr = (in_addr_t) s4->sin_addr.s_addr &
                                  (in_addr_t) s4_mask->sin_addr.s_addr;
                q = inet_ntop (AF_INET, &net_addr, net, sizeof (net));

                if (device->iface_name == NULL)
                        device->iface_name = g_strdup (ifa->ifa_name);
                if (device->network == NULL)
                        device->network = g_strdup (q);
                break;
        }

        g_list_free (up_ifaces);
        freeifaddrs (ifa_list);

        return TRUE;
}

