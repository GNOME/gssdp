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

#ifndef GSSDP_NET_H
#define GSSDP_NET_H

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#include <glib.h>
#include <gio/gio.h>

#ifdef G_OS_WIN32
#include <winsock2.h>
typedef unsigned long in_addr_t;
#else
#include <netinet/in.h>
#endif

struct _GSSDPNetworkDevice {
        char *iface_name;
        char *host_ip;
        GInetAddress *host_addr;
        GInetAddressMask *host_mask;
        GSocketFamily address_family;
        char *network;
        gint index;
};
typedef struct _GSSDPNetworkDevice GSSDPNetworkDevice;

G_GNUC_INTERNAL gboolean
gssdp_net_init                  (GError **error);

G_GNUC_INTERNAL void
gssdp_net_shutdown              (void);

G_GNUC_INTERNAL gboolean
gssdp_net_get_host_ip           (GSSDPNetworkDevice *device);

G_GNUC_INTERNAL int
gssdp_net_query_ifindex         (GSSDPNetworkDevice *device);

G_GNUC_INTERNAL char*
gssdp_net_mac_lookup            (GSSDPNetworkDevice *device,
                                 const char *ip_address);

G_GNUC_INTERNAL GList*
gssdp_net_list_devices          (void);

#endif /* GSSDP_NET_H */
