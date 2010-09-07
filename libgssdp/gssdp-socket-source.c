/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <glib.h>

#include "gssdp-socket-functions.h"
#include "gssdp-socket-source.h"
#include "gssdp-protocol.h"
#include "gssdp-error.h"

static void
gssdp_socket_source_initable_init (gpointer g_iface,
                                   gpointer iface_data);

G_DEFINE_TYPE_EXTENDED (GSSDPSocketSource,
                        gssdp_socket_source,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                    gssdp_socket_source_initable_init));

struct _GSSDPSocketSourcePrivate {
        GSource              *source;
        GSocket              *socket;
        GSSDPSocketSourceType type;
        char                 *host_ip;
};

enum {
    PROP_0,
    PROP_TYPE,
    PROP_HOST_IP
};

static void
gssdp_socket_source_init (GSSDPSocketSource *self)
{
        self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                  GSSDP_TYPE_SOCKET_SOURCE,
                                                  GSSDPSocketSourcePrivate);
}

static gboolean
gssdp_socket_source_do_init (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error);

static void
gssdp_socket_source_initable_init (gpointer g_iface,
                                   gpointer iface_data)
{
        GInitableIface *iface = (GInitableIface *)g_iface;
        iface->init = gssdp_socket_source_do_init;
}

static void
gssdp_socket_source_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
        GSSDPSocketSource *self;

        self = GSSDP_SOCKET_SOURCE (object);

        /* All properties are construct-only, write-only */
        switch (property_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_socket_source_set_property (GObject          *object,
                                  guint             property_id,
                                  const GValue     *value,
                                  GParamSpec       *pspec)
{
        GSSDPSocketSource *self;

        self = GSSDP_SOCKET_SOURCE (object);

        switch (property_id) {
        case PROP_TYPE:
                self->priv->type = g_value_get_int (value);
                break;
        case PROP_HOST_IP:
                self->priv->host_ip = g_value_dup_string (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

/**
 * gssdp_socket_source_new
 *
 * Return value: A new #GSSDPSocketSource
 **/
GSSDPSocketSource *
gssdp_socket_source_new (GSSDPSocketSourceType type,
                         const char           *host_ip,
                         GError              **error)
{
        return g_initable_new (GSSDP_TYPE_SOCKET_SOURCE,
                               NULL,
                               error,
                               "type",
                               type,
                               "host-ip",
                               host_ip,
                               NULL);
}

static gboolean
gssdp_socket_source_do_init (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error)
{
        GSSDPSocketSource *self = NULL;
        GInetAddress *iface_address = NULL;
        GSocketAddress *bind_address = NULL;
        GInetAddress *group = NULL;
        GError *inner_error = NULL;
        GSocketFamily family;

        self = GSSDP_SOCKET_SOURCE (initable);
        iface_address = g_inet_address_new_from_string (self->priv->host_ip);
        if (iface_address == NULL) {
                if (error != NULL) {
                        *error = g_error_new (GSSDP_ERROR,
                                              GSSDP_ERROR_FAILED,
                                              "Invalid host ip: %s",
                                              self->priv->host_ip);
                        inner_error = *error;
                }

                goto error;
        }

        family = g_inet_address_get_family (iface_address);

        if (family == G_SOCKET_FAMILY_IPV4)
                group = g_inet_address_new_from_string (SSDP_ADDR);
        else {
                if (error != NULL) {
                        *error = g_error_new_literal (GSSDP_ERROR,
                                                      GSSDP_ERROR_FAILED,
                                                      "IPv6 address");
                        inner_error = *error;
                }

                goto error;
        }


        /* Create socket */
        self->priv->socket = g_socket_new (G_SOCKET_FAMILY_IPV4,
                                           G_SOCKET_TYPE_DATAGRAM,
                                           G_SOCKET_PROTOCOL_UDP,
                                           &inner_error);

        if (!self->priv->socket) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Could not create socket");

                goto error;
        }

        /* Enable broadcasting */
        if (!gssdp_socket_enable_broadcast (self->priv->socket,
                                            TRUE,
                                            &inner_error)) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Failed to enable broadcast");
                goto error;
        }

        /* TTL */
        if (!gssdp_socket_set_ttl (self->priv->socket,
                                   4,
                                   &inner_error)) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Failed to set TTL");

        }
        /* Set up additional things according to the type of socket desired */
        if (self->priv->type == GSSDP_SOCKET_SOURCE_TYPE_MULTICAST) {
                /* Enable multicast loopback */
                if (!gssdp_socket_enable_loop (self->priv->socket,
                                               TRUE,
                                               &inner_error)) {
                        g_propagate_prefixed_error (
                                        error,
                                        inner_error,
                                        "Failed to enable loop-back");

                        goto error;
                }

                if (!gssdp_socket_mcast_interface_set (self->priv->socket,
                                                       iface_address,
                                                       &inner_error)) {
                        g_propagate_prefixed_error (
                                        error,
                                        inner_error,
                                        "Failed to set multicast interface");

                        goto error;
                }

#ifdef G_OS_WIN32
                bind_address = g_inet_socket_address_new (iface_address,
                                                          SSDP_PORT);
#else
                bind_address = g_inet_socket_address_new (group,
                                                          SSDP_PORT);
#endif
        } else {
                bind_address = g_inet_socket_address_new (iface_address,
                                                          SSDP_PORT);
        }

        /* Bind to requested port and address */
        if (!g_socket_bind (self->priv->socket,
                            bind_address,
                            TRUE,
                            &inner_error)) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Failed to bind socket");

                goto error;
        }

        if (self->priv->type == GSSDP_SOCKET_SOURCE_TYPE_MULTICAST) {

                 /* Subscribe to multicast channel */
                if (!gssdp_socket_mcast_group_join (self->priv->socket,
                                                    group,
                                                    iface_address,
                                                    &inner_error)) {
                        char *address = g_inet_address_to_string (group);
                        g_propagate_prefixed_error (error,
                                                    inner_error,
                                                    "Failed to join group %s",
                                                    address);
                        g_free (address);

                        goto error;
                }
        }

        self->priv->source = g_socket_create_source (self->priv->socket,
                                                     G_IO_IN | G_IO_ERR,
                                                     NULL);
error:
        if (iface_address != NULL)
                g_object_unref (iface_address);
        if (bind_address != NULL)
                g_object_unref (bind_address);
        if (group != NULL)
                g_object_unref (group);
        if (inner_error != NULL) {
                if (error == NULL) {
                        g_warning ("Failed to create socket source: %s",
                                   inner_error->message);
                        g_error_free (inner_error);
                }

                return FALSE;
        }

        return TRUE;
}

GSocket *
gssdp_socket_source_get_socket (GSSDPSocketSource *socket_source)
{
        g_return_val_if_fail (socket_source != NULL, NULL);

        return socket_source->priv->socket;
}

void
gssdp_socket_source_set_callback (GSSDPSocketSource *self,
                                  GSourceFunc        callback,
                                  gpointer           user_data)
{
        g_return_if_fail (self != NULL);
        g_return_if_fail (GSSDP_IS_SOCKET_SOURCE (self));

        g_source_set_callback (self->priv->source, callback, user_data, NULL);
}

void
gssdp_socket_source_attach (GSSDPSocketSource *self,
                            GMainContext      *context)
{
        g_return_if_fail (self != NULL);
        g_return_if_fail (GSSDP_IS_SOCKET_SOURCE (self));

        g_source_attach (self->priv->source, context);
}

static void
gssdp_socket_source_dispose (GObject *object)
{
        GSSDPSocketSource *self;

        self = GSSDP_SOCKET_SOURCE (object);

        if (self->priv->source != NULL) {
                g_source_unref (self->priv->source);
                g_source_destroy (self->priv->source);
                self->priv->source = NULL;
        }

        if (self->priv->socket != NULL) {
                g_socket_close (self->priv->socket, NULL);
                g_object_unref (self->priv->socket);
                self->priv->socket = NULL;
        }

        G_OBJECT_CLASS (gssdp_socket_source_parent_class)->dispose (object);
}

static void
gssdp_socket_source_finalize (GObject *object)
{
        GSSDPSocketSource *self;

        self = GSSDP_SOCKET_SOURCE (object);

        if (self->priv->host_ip != NULL) {
                g_free (self->priv->host_ip);
                self->priv->host_ip = NULL;
        }

        G_OBJECT_CLASS (gssdp_socket_source_parent_class)->finalize (object);
}

static void
gssdp_socket_source_class_init (GSSDPSocketSourceClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = gssdp_socket_source_get_property;
        object_class->set_property = gssdp_socket_source_set_property;
        object_class->dispose = gssdp_socket_source_dispose;
        object_class->finalize = gssdp_socket_source_finalize;

        g_type_class_add_private (klass, sizeof (GSSDPSocketSourcePrivate));

        g_object_class_install_property
                (object_class,
                 PROP_TYPE,
                 g_param_spec_int
                        ("type",
                         "Type",
                         "Type of socket-source (Multicast/Unicast)",
                         GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                         GSSDP_SOCKET_SOURCE_TYPE_MULTICAST,
                         GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                         G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_HOST_IP,
                 g_param_spec_string
                        ("host-ip",
                         "Host ip",
                         "IP address of associated network interface",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                         G_PARAM_STATIC_BLURB));
}
