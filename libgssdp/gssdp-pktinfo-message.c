/*
 * Copyright (C) 2014 Jens Georg.
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

#include <netinet/ip.h>

#include "gssdp-pktinfo-message.h"

G_DEFINE_TYPE (GSSDPPktinfoMessage,
               gssdp_pktinfo_message,
               G_TYPE_SOCKET_CONTROL_MESSAGE)

struct _GSSDPPktinfoMessagePrivate
{
        GInetAddress *pkt_addr;
        GInetAddress *iface_addr;
        gint          index;
};

static gsize
gssdp_pktinfo_message_get_size (GSocketControlMessage *msg)
{
    return 0;
}

static int
gssdp_pktinfo_message_get_level (GSocketControlMessage *msg)
{
        return IPPROTO_IP;
}

static int
gssdp_pktinfo_message_get_msg_type (GSocketControlMessage *msg)
{
        return IP_PKTINFO;
}

static GSocketControlMessage *
gssdp_pktinfo_message_deserialize (int      level,
                                   int      type,
                                   gsize    size,
                                   gpointer data)
{
        GSocketControlMessage *message;
        GInetAddress *addr, *dst;
        struct in_pktinfo *info = (struct in_pktinfo *) data;
        const guint8 *bytes;

        if (level != IPPROTO_IP || type != IP_PKTINFO)
            return NULL;

        bytes = (const guint8 *)&(info->ipi_addr.s_addr);
        addr = g_inet_address_new_from_bytes(bytes, G_SOCKET_FAMILY_IPV4);

        bytes = (const guint8 *)&(info->ipi_spec_dst.s_addr);
        dst = g_inet_address_new_from_bytes (bytes, G_SOCKET_FAMILY_IPV4);

        message = gssdp_pktinfo_message_new (addr, dst, info->ipi_ifindex);

        return message;
}

static void
gssdp_pktinfo_message_init (GSSDPPktinfoMessage *self)
{
        self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                  GSSDP_TYPE_PKTINFO_MESSAGE,
                                                  GSSDPPktinfoMessagePrivate);
}

static void
gssdp_pktinfo_message_class_init (GSSDPPktinfoMessageClass *klass)
{
        GSocketControlMessageClass *scm_class =
                G_SOCKET_CONTROL_MESSAGE_CLASS (klass);

        g_type_class_add_private (klass, sizeof (GSSDPPktinfoMessagePrivate));

        scm_class->get_size = gssdp_pktinfo_message_get_size;
        scm_class->get_level = gssdp_pktinfo_message_get_level;
        scm_class->get_type = gssdp_pktinfo_message_get_msg_type;
        scm_class->deserialize = gssdp_pktinfo_message_deserialize;
}

GSocketControlMessage *
gssdp_pktinfo_message_new (GInetAddress *addr, GInetAddress *dst, gint ifindex)
{
        GSSDPPktinfoMessage *msg;

        msg = GSSDP_PKTINFO_MESSAGE (g_object_new (GSSDP_TYPE_PKTINFO_MESSAGE,
                                                   NULL));

        /* FIXME: Use properties */
        msg->priv->pkt_addr = addr;
        msg->priv->iface_addr = dst;
        msg->priv->index = ifindex;

        return G_SOCKET_CONTROL_MESSAGE (msg);
}

gint
gssdp_pktinfo_message_get_ifindex (GSSDPPktinfoMessage *message)
{
        g_return_val_if_fail (GSSDP_IS_PKTINFO_MESSAGE (message), -1);

        return message->priv->index;
}

GInetAddress *
gssdp_pktinfo_message_get_local_addr (GSSDPPktinfoMessage *message)
{
        return message->priv->iface_addr;
}

GInetAddress *
gssdp_pktinfo_message_get_pkt_addr (GSSDPPktinfoMessage *message)
{
        return message->priv->pkt_addr;
}
