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

#ifndef __GSSDP_SOCKET_SOURCE_H__
#define __GSSDP_SOCKET_SOURCE_H__

#include <glib-object.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define GSSDP_TYPE_SOCKET_SOURCE \
                (gssdp_socket_source_get_type ())
#define GSSDP_SOCKET_SOURCE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GSSDP_TYPE_SOCKET_SOURCE, \
                 GSSDPSocketSource))
#define GSSDP_SOCKET_SOURCE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 GSSDP_TYPE_SOCKET_SOURCE, \
                 GSSDPSocketSourceClass))
#define GSSDP_IS_SOCKET_SOURCE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GSSDP_TYPE_SOCKET_SOURCE))
#define GSSDP_IS_SOCKET_SOURCE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 GSSDP_TYPE_SOCKET_SOURCE))
#define GSSDP_SOCKET_SOURCE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GSSDP_TYPE_SOCKET_SOURCE, \
                 GSSDPSocketSourceClass))

typedef struct _GSSDPSocketSourcePrivate GSSDPSocketSourcePrivate;

typedef enum {
        GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
        GSSDP_SOCKET_SOURCE_TYPE_MULTICAST,
        GSSDP_SOCKET_SOURCE_TYPE_SEARCH
} GSSDPSocketSourceType;



typedef struct _GSSDPSocketSource {
        GObject                   parent;

        GSSDPSocketSourcePrivate *priv;
} GSSDPSocketSource;

typedef struct _GSSDPSocketSourceClass {
        GObjectClass parent_class;
} GSSDPSocketSourceClass;

G_GNUC_INTERNAL GSSDPSocketSource *
gssdp_socket_source_new        (GSSDPSocketSourceType  type,
                                const char            *host_ip,
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

#endif /* __GSSDP_SOCKET_SOURCE_H__ */
