/*
 * Copyright (C) 2014 Jens Georg.
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>

#include "gssdp-pktinfo-message.h"

#include <netinet/ip.h>

struct _GSSDPPktinfoMessage {
        GSocketControlMessage parent;
};

struct _GSSDPPktinfoMessageClass {
        GSocketControlMessageClass parent_class;
};

struct _GSSDPPktinfoMessagePrivate
{
        GInetAddress *pkt_addr;
        GInetAddress *iface_addr;
        gint          index;
};
typedef struct _GSSDPPktinfoMessagePrivate GSSDPPktinfoMessagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GSSDPPktinfoMessage,
                            gssdp_pktinfo_message,
                            G_TYPE_SOCKET_CONTROL_MESSAGE)

enum {
        PROP_0,
        PROP_PKT_ADDR,
        PROP_IFACE_ADDR,
        PROP_INDEX
};

static gsize
gssdp_pktinfo_message_get_size (GSocketControlMessage *msg)
{
    return sizeof (struct in_pktinfo);
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

static void
gssdp_pktinfo_message_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        GSSDPPktinfoMessage *self;
        GSSDPPktinfoMessagePrivate *priv;

        self = GSSDP_PKTINFO_MESSAGE (object);
        priv = gssdp_pktinfo_message_get_instance_private (self);

        switch (property_id)
        {
        case PROP_IFACE_ADDR:
                g_value_set_object (value, priv->iface_addr);
                break;
        case PROP_INDEX:
                g_value_set_int (value, priv->index);
                break;
        case PROP_PKT_ADDR:
                g_value_set_object (value, priv->pkt_addr);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_pktinfo_message_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        GSSDPPktinfoMessage *self;
        GSSDPPktinfoMessagePrivate *priv;

        self = GSSDP_PKTINFO_MESSAGE (object);
        priv = gssdp_pktinfo_message_get_instance_private (self);
        switch (property_id)
        {
        case PROP_IFACE_ADDR:
                priv->iface_addr = g_value_get_object (value);
                break;
        case PROP_INDEX:
                priv->index = g_value_get_int (value);
                break;
        case PROP_PKT_ADDR:
                priv->pkt_addr = g_value_get_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_pktinfo_dispose (GObject *object)
{
        GSSDPPktinfoMessage *self = GSSDP_PKTINFO_MESSAGE (object);
        GSSDPPktinfoMessagePrivate *priv = NULL;

        priv = gssdp_pktinfo_message_get_instance_private (self);

        g_clear_object (&priv->iface_addr);
        g_clear_object (&priv->pkt_addr);
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
}

static void
gssdp_pktinfo_message_class_init (GSSDPPktinfoMessageClass *klass)
{
        GSocketControlMessageClass *scm_class =
                G_SOCKET_CONTROL_MESSAGE_CLASS (klass);

        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        scm_class->get_size = gssdp_pktinfo_message_get_size;
        scm_class->get_level = gssdp_pktinfo_message_get_level;
        scm_class->get_type = gssdp_pktinfo_message_get_msg_type;
        scm_class->deserialize = gssdp_pktinfo_message_deserialize;

        object_class->get_property = gssdp_pktinfo_message_get_property;
        object_class->set_property = gssdp_pktinfo_message_set_property;
        object_class->dispose = gssdp_pktinfo_dispose;

        g_object_class_install_property
                (object_class,
                 PROP_IFACE_ADDR,
                 g_param_spec_object ("iface-address",
                                      "iface-address",
                                      "IP v4 Address of the interface this packet was received on",
                                      G_TYPE_INET_ADDRESS,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT |
                                      G_PARAM_STATIC_STRINGS));

        g_object_class_install_property
                (object_class,
                 PROP_INDEX,
                 g_param_spec_int ("index",
                                    "index",
                                    "Network interface index",
                                    0,
                                    G_MAXINT,
                                    0,
                                    G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT |
                                    G_PARAM_STATIC_STRINGS));

        g_object_class_install_property
                (object_class,
                 PROP_PKT_ADDR,
                 g_param_spec_object ("pkt-address",
                                      "pkt-address",
                                      "IP v4 destination Address of the packet",
                                      G_TYPE_INET_ADDRESS,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT |
                                      G_PARAM_STATIC_STRINGS));
}

GSocketControlMessage *
gssdp_pktinfo_message_new (GInetAddress *addr, GInetAddress *dst, gint ifindex)
{
        GSSDPPktinfoMessage *msg;

        msg = GSSDP_PKTINFO_MESSAGE (
                g_object_new (GSSDP_TYPE_PKTINFO_MESSAGE,
                              "iface-address", dst,
                              "index", ifindex,
                              "pkt-address", addr,
                              NULL));

        return G_SOCKET_CONTROL_MESSAGE (msg);
}

gint
gssdp_pktinfo_message_get_ifindex (GSSDPPktinfoMessage *message)
{
        GSSDPPktinfoMessagePrivate *priv;
        g_return_val_if_fail (GSSDP_IS_PKTINFO_MESSAGE (message), -1);

        priv = gssdp_pktinfo_message_get_instance_private (message);

        return priv->index;
}

GInetAddress *
gssdp_pktinfo_message_get_local_addr (GSSDPPktinfoMessage *message)
{
        GSSDPPktinfoMessagePrivate *priv;
        g_return_val_if_fail (GSSDP_IS_PKTINFO_MESSAGE (message), NULL);

        priv = gssdp_pktinfo_message_get_instance_private (message);

        return priv->iface_addr;
}

GInetAddress *
gssdp_pktinfo_message_get_pkt_addr (GSSDPPktinfoMessage *message)
{
        GSSDPPktinfoMessagePrivate *priv;
        g_return_val_if_fail (GSSDP_IS_PKTINFO_MESSAGE (message), NULL);

        priv = gssdp_pktinfo_message_get_instance_private (message);

        return priv->pkt_addr;
}
