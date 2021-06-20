/*
 * Copyright (C) 2014 Jens Georg.
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define _GNU_SOURCE

#ifdef __APPLE__
#define __APPLE_USE_RFC_3542
#endif

#include <sys/socket.h>
#include <netinet/ip.h>

#include "gssdp-pktinfo6-message.h"

typedef struct _GSSDPPktinfo6MessagePrivate GSSDPPktinfo6MessagePrivate;
struct _GSSDPPktinfo6MessagePrivate
{
        GInetAddress *iface_addr;
        gint          index;
};

struct _GSSDPPktinfo6Message {
    GSocketControlMessage parent_instance;

    GSSDPPktinfo6MessagePrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GSSDPPktinfo6Message,
                            gssdp_pktinfo6_message,
                            G_TYPE_SOCKET_CONTROL_MESSAGE)

enum {
        PROP_0,
        PROP_IFACE_ADDR,
        PROP_INDEX
};

static gsize
gssdp_pktinfo6_message_get_size (GSocketControlMessage *msg)
{
    return sizeof (struct in6_pktinfo);
}

static int
gssdp_pktinfo6_message_get_level (GSocketControlMessage *msg)
{
        return IPPROTO_IPV6;
}

static int
gssdp_pktinfo6_message_get_msg_type (GSocketControlMessage *msg)
{
        return IPV6_PKTINFO;
}

static void
gssdp_pktinfo6_message_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        GSSDPPktinfo6Message *self;

        self = GSSDP_PKTINFO6_MESSAGE (object);
        switch (property_id)
        {
        case PROP_IFACE_ADDR:
                g_value_set_object (value, self->priv->iface_addr);
                break;
        case PROP_INDEX:
                g_value_set_int (value, self->priv->index);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_pktinfo6_message_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        GSSDPPktinfo6Message *self;

        self = GSSDP_PKTINFO6_MESSAGE (object);
        switch (property_id)
        {
        case PROP_IFACE_ADDR:
                self->priv->iface_addr = g_value_get_object (value);
                break;
        case PROP_INDEX:
                self->priv->index = g_value_get_int (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_pktinfo6_dispose (GObject *object)
{
        GSSDPPktinfo6Message *self = GSSDP_PKTINFO6_MESSAGE (object);

        g_clear_object (&self->priv->iface_addr);
}

static GSocketControlMessage *
gssdp_pktinfo6_message_deserialize (int      level,
                                    int      type,
                                    gsize    size,
                                    gpointer data)
{
        GSocketControlMessage *message;
        GInetAddress *addr;
        struct in6_pktinfo *info = (struct in6_pktinfo *) data;
        const guint8 *bytes;

        if (level != IPPROTO_IPV6 || type != IPV6_PKTINFO)
                return NULL;

        bytes = (const guint8 *)&(info->ipi6_addr);
        addr = g_inet_address_new_from_bytes (bytes, G_SOCKET_FAMILY_IPV6);

        message = gssdp_pktinfo6_message_new (addr, info->ipi6_ifindex);

        return message;
}

static void
gssdp_pktinfo6_message_init (GSSDPPktinfo6Message *self)
{
        self->priv = gssdp_pktinfo6_message_get_instance_private (self);
}

static void
gssdp_pktinfo6_message_class_init (GSSDPPktinfo6MessageClass *klass)
{
        GSocketControlMessageClass *scm_class =
                G_SOCKET_CONTROL_MESSAGE_CLASS (klass);

        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        scm_class->get_size = gssdp_pktinfo6_message_get_size;
        scm_class->get_level = gssdp_pktinfo6_message_get_level;
        scm_class->get_type = gssdp_pktinfo6_message_get_msg_type;
        scm_class->deserialize = gssdp_pktinfo6_message_deserialize;

        object_class->get_property = gssdp_pktinfo6_message_get_property;
        object_class->set_property = gssdp_pktinfo6_message_set_property;
        object_class->dispose = gssdp_pktinfo6_dispose;

        g_object_class_install_property
                (object_class,
                 PROP_IFACE_ADDR,
                 g_param_spec_object ("iface-address",
                                      "iface-address",
                                      "IP v6 Address of the interface this packet was received on",
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

}

GSocketControlMessage *
gssdp_pktinfo6_message_new (GInetAddress *addr, gint ifindex)
{
        GSSDPPktinfo6Message *msg;

        msg = GSSDP_PKTINFO6_MESSAGE (
                g_object_new (GSSDP_TYPE_PKTINFO6_MESSAGE,
                              "iface-address", addr,
                              "index", ifindex,
                              NULL));

        return G_SOCKET_CONTROL_MESSAGE (msg);
}

gint
gssdp_pktinfo6_message_get_ifindex (GSSDPPktinfo6Message *message)
{
        g_return_val_if_fail (GSSDP_IS_PKTINFO6_MESSAGE (message), -1);

        return message->priv->index;
}

GInetAddress *
gssdp_pktinfo6_message_get_local_addr (GSSDPPktinfo6Message *message)
{
        g_return_val_if_fail (GSSDP_IS_PKTINFO6_MESSAGE (message), NULL);

        return message->priv->iface_addr;
}
