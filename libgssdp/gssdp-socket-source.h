/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Jens Georg <mail@jensge.org>
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
                                const char            *host_ip,
                                guint                  ttl,
                                const char            *device_name,
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
