/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#ifndef GSSDP_SOCKET_SOURCE_H
#define GSSDP_SOCKET_SOURCE_H

#include <glib-object.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define GSSDP_TYPE_SOCKET_SOURCE (gssdp_socket_source_get_type ())

G_DECLARE_FINAL_TYPE (GSSDPSocketSource,
                      gssdp_socket_source,
                      GSSDP,
                      SOCKET_SOURCE,
                      GObject)

typedef enum {
        GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
        GSSDP_SOCKET_SOURCE_TYPE_MULTICAST,
        GSSDP_SOCKET_SOURCE_TYPE_SEARCH
} GSSDPSocketSourceType;

G_GNUC_INTERNAL GSSDPSocketSource *
gssdp_socket_source_new        (GSSDPSocketSourceType  type,
                                GInetAddress          *address,
                                guint                  ttl,
                                const char            *device_name,
                                guint                  index,
                                GError               **error);

G_GNUC_INTERNAL GSocket*
gssdp_socket_source_get_socket (GSSDPSocketSource     *socket_source);

G_GNUC_INTERNAL void
gssdp_socket_source_set_callback (GSSDPSocketSource   *socket_source,
                                  GSourceFunc          callback,
                                  gpointer             user_data);

G_GNUC_INTERNAL void
gssdp_socket_source_attach       (GSSDPSocketSource   *socket_source);

G_END_DECLS

#endif /* GSSDP_SOCKET_SOURCE_H */
