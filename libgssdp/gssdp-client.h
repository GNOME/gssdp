/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#ifndef GSSDP_CLIENT_H
#define GSSDP_CLIENT_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GSSDP_TYPE_CLIENT (gssdp_client_get_type ())

G_DECLARE_DERIVABLE_TYPE (GSSDPClient, gssdp_client, GSSDP, CLIENT, GObject)

typedef struct _GSSDPClient GSSDPClient;
typedef struct _GSSDPClientClass GSSDPClientClass;

/**
 * GSSDPUDAVersion:
 * @GSSDP_UDA_VERSION_UNSPECIFIED: When creating a client, use the default version
 * @GSSDP_UDA_VERSION_1_0: Use Version 1.0 of the UDA specification (UPnP/1.0)
 * @GSSDP_UDA_VERSION_1_1: Use Version 1.1 of the UDA specification (UPnP/1.1)
 *
 * Implemented behavior of the UDA (Unified Device Architecture) protocol.
 */
typedef enum /*< prefix=GSSDP_UDA_ >*/
{
        GSSDP_UDA_VERSION_UNSPECIFIED,
        GSSDP_UDA_VERSION_1_0,
        GSSDP_UDA_VERSION_1_1,
} GSSDPUDAVersion;

struct _GSSDPClientClass {
        GObjectClass parent_class;

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
};

G_DEPRECATED_FOR(gssdp_client_new_for_address)
GSSDPClient *
gssdp_client_new              (const char   *iface,
                               GError      **error);

G_DEPRECATED_FOR(gssdp_client_new_for_address)
GSSDPClient *
gssdp_client_new_with_port    (const char *iface,
                               guint16     msearch_port,
                               GError    **error);

GSSDPClient *
gssdp_client_new_for_address (GInetAddress *addr,
                              guint16 port,
                              GSSDPUDAVersion uda_version,
                              GError **error);

GSSDPClient *
gssdp_client_new_full (const char *iface,
                       GInetAddress *addr,
                       guint16 port,
                       GSSDPUDAVersion uda_version,
                       GError **error);

void
gssdp_client_set_server_id    (GSSDPClient  *client,
                               const char   *server_id);

const char *
gssdp_client_get_server_id    (GSSDPClient  *client);

const char *
gssdp_client_get_interface    (GSSDPClient  *client);

const char *
gssdp_client_get_host_ip      (GSSDPClient  *client);

void
gssdp_client_set_network      (GSSDPClient  *client,
                               const char   *network);

const char *
gssdp_client_get_network      (GSSDPClient  *client);

gboolean
gssdp_client_get_active       (GSSDPClient  *client);

GInetAddress *
gssdp_client_get_address      (GSSDPClient *client);

guint
gssdp_client_get_index        (GSSDPClient *client);

GSocketFamily
gssdp_client_get_family       (GSSDPClient *client);

guint
gssdp_client_get_port (GSSDPClient *client);

GInetAddressMask *
gssdp_client_get_address_mask (GSSDPClient *client);

void
gssdp_client_append_header    (GSSDPClient *client,
                               const char  *name,
                               const char  *value);

void
gssdp_client_remove_header    (GSSDPClient *client,
                               const char  *name);

void
gssdp_client_clear_headers    (GSSDPClient *client);

void
gssdp_client_add_cache_entry  (GSSDPClient  *client,
                               const char   *ip_address,
                               const char   *user_agent);

const char *
gssdp_client_guess_user_agent (GSSDPClient *client,
                               const char  *ip_address);

GSSDPUDAVersion
gssdp_client_get_uda_version  (GSSDPClient *client);

void
gssdp_client_set_boot_id      (GSSDPClient *client,
                               gint32       boot_id);

void
gssdp_client_set_config_id    (GSSDPClient *client,
                               gint32       config_id);

gboolean
gssdp_client_can_reach (GSSDPClient *client,
                        GInetSocketAddress *address);

G_END_DECLS

#endif /* GSSDP_CLIENT_H */
