/*
 * Copyright (C) 2014 Jens Georg <mail@jensge.org>
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

#ifndef __GSSDP_PKTINFO_MESSAGE_H__
#define __GSSDP_PKTINFO_MESSAGE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL GType
gssdp_pktinfo_message_get_type (void) G_GNUC_CONST;

#define GSSDP_TYPE_PKTINFO_MESSAGE (gssdp_pktinfo_message_get_type())
#define GSSDP_PKTINFO_MESSAGE(obj) \
                            (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                             GSSDP_TYPE_PKTINFO_MESSAGE, \
                             GSSDPPktinfoMessage))
#define GSSDP_PKTINFO_MESAGE_CLASS(klass) \
                            (G_TYPE_CHECK_CLASS_CAST ((klass), \
                             GSSDP_TYPE_PKTINFO_MESSAGE, \
                             GSSDPPktinfoClass))
#define GSSDP_IS_PKTINFO_MESSAGE(obj) \
                            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                             GSSDP_TYPE_PKTINFO_MESSAGE))
#define GSSDP_IS_PKTINFO_MESSAGE_CLASS(klass) \
                            (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                             GSSDP_TYPE_PKTINFO_MESSAGE))
#define GSSDP_PKTINFO_MESSAGE_GET_CLASS(obj) \
                            (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                             GSSDP_TYPE_PKTINFO_MESSAGE, \
                             GSSDPPktinfoMessageClass))

typedef struct _GSSDPPktinfoMessagePrivate GSSDPPktinfoMessagePrivate;
typedef struct _GSSDPPktInfoMessage GSSDPPktinfoMessage;
typedef struct _GSSDPPktinfoMessageClass GSSDPPktinfoMessageClass;

struct _GSSDPPktInfoMessage {
        GSocketControlMessage parent;
        GSSDPPktinfoMessagePrivate *priv;
};

struct _GSSDPPktinfoMessageClass {
        GSocketControlMessageClass parent_class;
};

G_GNUC_INTERNAL GSocketControlMessage *
gssdp_pktinfo_message_new (GInetAddress *addr, GInetAddress *dst, gint ifindex);

G_GNUC_INTERNAL gint
gssdp_pktinfo_message_get_ifindex (GSSDPPktinfoMessage *message);

G_GNUC_INTERNAL GInetAddress *
gssdp_pktinfo_message_get_local_addr (GSSDPPktinfoMessage *message);

G_GNUC_INTERNAL GInetAddress *
gssdp_pktinfo_message_get_pkt_addr (GSSDPPktinfoMessage *message);

#endif /* __GSSDP_PKTINFO_MESSAGE_H__ */
