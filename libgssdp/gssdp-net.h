/*
 * Copyright (C) 2016 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
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
gssdp_net_get_host_ip           (GSSDPNetworkDevice *device, GError **error);

G_GNUC_INTERNAL int
gssdp_net_query_ifindex         (GSSDPNetworkDevice *device);

G_GNUC_INTERNAL char*
gssdp_net_mac_lookup            (GSSDPNetworkDevice *device,
                                 const char *ip_address);

G_GNUC_INTERNAL GList*
gssdp_net_list_devices          (void);

#endif /* GSSDP_NET_H */
