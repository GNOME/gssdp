/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
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

#ifndef __GSSDP_CLIENT_H__
#define __GSSDP_CLIENT_H__

#include <glib-object.h>

G_BEGIN_DECLS

GType
gssdp_client_get_type (void) G_GNUC_CONST;

#define GSSDP_TYPE_CLIENT \
                (gssdp_client_get_type ())
#define GSSDP_CLIENT(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GSSDP_TYPE_CLIENT, \
                 GSSDPClient))
#define GSSDP_CLIENT_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 GSSDP_TYPE_CLIENT, \
                 GSSDPClientClass))
#define GSSDP_IS_CLIENT(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GSSDP_TYPE_CLIENT))
#define GSSDP_IS_CLIENT_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 GSSDP_TYPE_CLIENT))
#define GSSDP_CLIENT_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GSSDP_TYPE_CLIENT, \
                 GSSDPClientClass))

typedef struct _GSSDPClientPrivate GSSDPClientPrivate;
typedef struct _GSSDPClient GSSDPClient;
typedef struct _GSSDPClientClass GSSDPClientClass;

struct _GSSDPClient {
        GObject parent;

        GSSDPClientPrivate *priv;
};

struct _GSSDPClientClass {
        GObjectClass parent_class;

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
};

GSSDPClient *
gssdp_client_new              (GMainContext *main_context,
                               const char   *iface,
                               GError      **error);

GSSDPClient *
gssdp_client_new_with_port    (const char *iface,
                               guint16     msearch_port,
                               GError    **error);

#ifndef GSSDP_DISABLE_DEPRECATED
GMainContext *
gssdp_client_get_main_context (GSSDPClient  *client);
#endif

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

G_END_DECLS

#endif /* __GSSDP_CLIENT_H__ */
