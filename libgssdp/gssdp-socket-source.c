/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
 *         Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>

#include "gssdp-socket-functions.h"
#include "gssdp-socket-source.h"
#include "gssdp-protocol.h"
#include "gssdp-error.h"

#include <glib.h>
#include <gio/gio.h>

struct _GSSDPSocketSource {
        GObject parent;
};

struct _GSSDPSocketSourceClass {
        GObjectClass parent_class;
};

struct _GSSDPSocketSourcePrivate {
        GSource              *source;
        GSocket              *socket;
        GSSDPSocketSourceType type;

        GInetAddress         *address;
        char                 *device_name;
        gint                  index;
        guint                 ttl;
        guint                 port;
};
typedef struct _GSSDPSocketSourcePrivate GSSDPSocketSourcePrivate;

static void
gssdp_socket_source_initable_init (gpointer g_iface,
                                   gpointer iface_data);

G_DEFINE_TYPE_EXTENDED (GSSDPSocketSource,
                        gssdp_socket_source,
                        G_TYPE_OBJECT,
                        0,
                        G_ADD_PRIVATE (GSSDPSocketSource)
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                    gssdp_socket_source_initable_init));

enum {
    PROP_0,
    PROP_TYPE,
    PROP_ADDRESS,
    PROP_TTL,
    PROP_PORT,
    PROP_IFA_NAME,
    PROP_IFA_IDX
};

static void
gssdp_socket_source_init (GSSDPSocketSource *self)
{
}

static gboolean
gssdp_socket_source_do_init (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error);

static void
gssdp_socket_source_initable_init (gpointer               g_iface,
                                   G_GNUC_UNUSED gpointer iface_data)
{
        GInitableIface *iface = (GInitableIface *)g_iface;
        iface->init = gssdp_socket_source_do_init;
}

static void
gssdp_socket_source_get_property (GObject              *object,
                                  guint                 property_id,
                                  G_GNUC_UNUSED GValue *value,
                                  GParamSpec           *pspec)
{
        GSSDPSocketSource *self;
        GSSDPSocketSourcePrivate *priv;

        self = GSSDP_SOCKET_SOURCE (object);
        priv = gssdp_socket_source_get_instance_private (self);

        /* All properties are construct-only, write-only */
        switch (property_id) {
        case PROP_PORT:
                g_value_set_uint (value, priv->port);
                break;
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
        GSSDPSocketSourcePrivate *priv;

        self = GSSDP_SOCKET_SOURCE (object);
        priv = gssdp_socket_source_get_instance_private (self);

        switch (property_id) {
        case PROP_TYPE:
                priv->type = g_value_get_int (value);
                break;
        case PROP_IFA_NAME:
                priv->device_name = g_value_dup_string (value);
                break;
        case PROP_ADDRESS:
                priv->address = g_value_dup_object (value);
                break;
        case PROP_TTL:
                priv->ttl = g_value_get_uint (value);
                break;
        case PROP_PORT:
                priv->port = g_value_get_uint (value);
                break;
        case PROP_IFA_IDX:
                priv->index = g_value_get_int (value);
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
                         GInetAddress         *address,
                         guint                 ttl,
                         const char           *device_name,
                         guint                 index,
                         GError              **error)
{
        return g_initable_new (GSSDP_TYPE_SOCKET_SOURCE,
                               NULL,
                               error,
                               "type",
                               type,
                               "address",
                               address,
                               "ttl",
                               ttl,
                               "device-name",
                               device_name,
                               "index",
                               index,
                               NULL);
}

static gboolean
gssdp_socket_source_do_init (GInitable                   *initable,
                             G_GNUC_UNUSED GCancellable  *cancellable,
                             GError                     **error)
{
        GSSDPSocketSource *self = NULL;
        GSSDPSocketSourcePrivate *priv = NULL;
        GSocketAddress *bind_address = NULL;
        GInetAddress *group = NULL;
        GError *inner_error = NULL;
        GSocketFamily family;
        gboolean success = FALSE;
        gboolean link_local = FALSE;

        self = GSSDP_SOCKET_SOURCE (initable);
        priv = gssdp_socket_source_get_instance_private (self);

        family = g_inet_address_get_family (priv->address);

        if (family == G_SOCKET_FAMILY_IPV4)
                group = g_inet_address_new_from_string (SSDP_ADDR);
        else {
                /* IPv6 */
                /* According to Annex.A, we need to check the scope of the
                 * address to use the proper multicast group */
                if (g_inet_address_get_is_link_local (priv->address)) {
                            group = g_inet_address_new_from_string (SSDP_V6_LL);
                            link_local = TRUE;
                } else {
                            group = g_inet_address_new_from_string (SSDP_V6_SL);
                }
        }


        /* Create socket */
        priv->socket = g_socket_new (family,
                                     G_SOCKET_TYPE_DATAGRAM,
                                     G_SOCKET_PROTOCOL_UDP,
                                     &inner_error);

        if (!priv->socket) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Could not create socket");

                goto error;
        }

        /* Enable broadcasting */
        g_socket_set_broadcast (priv->socket, TRUE);

        if (!gssdp_socket_enable_info (priv->socket,
                                       family,
                                       TRUE,
                                       &inner_error)) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Failed to enable info messages");

                goto error;
        }

        /* TTL */
        if (priv->ttl == 0) {
                /* UDA/1.0 says 4, UDA/1.1 says 2 */
                priv->ttl = 4;
                if (family == G_SOCKET_FAMILY_IPV6) {
                        /* UDA 2.0, Annex A says 10 hops */
                        priv->ttl = 10;
                }
        }

        g_socket_set_multicast_ttl (priv->socket, priv->ttl);


        /* Set up additional things according to the type of socket desired */
        if (priv->type == GSSDP_SOCKET_SOURCE_TYPE_MULTICAST) {
                /* Enable multicast loopback */
                g_socket_set_multicast_loopback (priv->socket, TRUE);


#ifdef G_OS_WIN32
                bind_address = g_inet_socket_address_new (priv->address,
                                                          SSDP_PORT);
#else
                bind_address = g_object_new (G_TYPE_INET_SOCKET_ADDRESS,
                                             "address", group,
                                             "port", SSDP_PORT,
                                             "scope-id", priv->index,
                                             NULL);
#endif
        } else {
                guint port = SSDP_PORT;

                if (family != G_SOCKET_FAMILY_IPV6 ||
                    (!g_inet_address_get_is_loopback (priv->address))) {

                        if (!gssdp_socket_mcast_interface_set (priv->socket,
                                                priv->address,
                                                (guint32) priv->index,
                                                &inner_error)) {
                                g_propagate_prefixed_error (
                                                error,
                                                inner_error,
                                                "Failed to set multicast interface");

                                goto error;
                        }
                }

                /* Use user-supplied or random port for the socket source used
                 * by M-SEARCH */
                if (priv->type == GSSDP_SOCKET_SOURCE_TYPE_SEARCH)
                        port = priv->port;

                if (link_local) {
                    bind_address = g_object_new (G_TYPE_INET_SOCKET_ADDRESS,
                                                 "address", priv->address,
                                                 "port", port,
                                                 "scope-id", priv->index,
                                                 NULL);
                } else {
                    bind_address = g_inet_socket_address_new (priv->address,
                                                              port);
                }

        }

        /* Normally g_socket_bind does this, but it is disabled on
         * windows since SO_REUSEADDR has different semantics
         * there, also we nees SO_REUSEPORT on OpenBSD. This is a nop
         * everywhere else.
         */
        if (!gssdp_socket_reuse_address (priv->socket,
                                         TRUE,
                                         &inner_error)) {
                g_propagate_prefixed_error (
                                error,
                                inner_error,
                                "Failed to enable reuse");

                goto error;
        }

        /* Bind to requested port and address */
        if (!g_socket_bind (priv->socket,
                            bind_address,
                            TRUE,
                            &inner_error)) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Failed to bind socket");

                goto error;
        }

        if (priv->type == GSSDP_SOCKET_SOURCE_TYPE_SEARCH && priv->port == 0) {
                GSocketAddress *addr =
                        g_socket_get_local_address (priv->socket, &inner_error);

                if (inner_error != NULL) {
                    g_propagate_prefixed_error (
                            error,
                            inner_error,
                            "Failed to get port from socket");
                }

                priv->port = g_inet_socket_address_get_port (
                        G_INET_SOCKET_ADDRESS (addr));
                g_object_unref (addr);
        }

        if (priv->type == GSSDP_SOCKET_SOURCE_TYPE_MULTICAST) {
                /* The 4th argument 'iface_name' can't be NULL even though Glib API doc says you
                 * can. 'NULL' will fail the test.
                 */
                if (!g_socket_join_multicast_group (priv->socket,
                                                    group,
                                                    FALSE,
                                                    priv->device_name,  /*   e.g. 'lo' */
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

        priv->source = g_socket_create_source (priv->socket,
                                               G_IO_IN | G_IO_ERR,
                                               NULL);
        success = TRUE;

error:
        g_clear_object (&bind_address);
        g_clear_object (&group);

        if (!success)
                /* Be aware that inner_error has already been free'd by
                 * g_propagate_error(), so we cannot access its contents
                 * anymore. */
                if (error == NULL)
                        g_warning ("Failed to create socket source");

        return success;
}

GSocket *
gssdp_socket_source_get_socket (GSSDPSocketSource *socket_source)
{
        GSSDPSocketSourcePrivate *priv;
        g_return_val_if_fail (socket_source != NULL, NULL);
        priv = gssdp_socket_source_get_instance_private (socket_source);

        return priv->socket;
}

void
gssdp_socket_source_set_callback (GSSDPSocketSource *self,
                                  GSourceFunc        callback,
                                  gpointer           user_data)
{
        GSSDPSocketSourcePrivate *priv;
        g_return_if_fail (self != NULL);
        g_return_if_fail (GSSDP_IS_SOCKET_SOURCE (self));
        priv = gssdp_socket_source_get_instance_private (self);

        g_source_set_callback (priv->source, callback, user_data, NULL);
}

void
gssdp_socket_source_attach (GSSDPSocketSource *self)
{
        GSSDPSocketSourcePrivate *priv;
        g_return_if_fail (self != NULL);
        g_return_if_fail (GSSDP_IS_SOCKET_SOURCE (self));
        priv = gssdp_socket_source_get_instance_private (self);

        g_source_attach (priv->source,
                         g_main_context_get_thread_default ());
}

static void
gssdp_socket_source_dispose (GObject *object)
{
        GSSDPSocketSource *self;
        GSSDPSocketSourcePrivate *priv;

        self = GSSDP_SOCKET_SOURCE (object);
        priv = gssdp_socket_source_get_instance_private (self);

        if (priv->source != NULL) {
                g_source_destroy (priv->source);
                g_source_unref (priv->source);
                priv->source = NULL;
        }

        if (priv->socket != NULL) {
                g_socket_close (priv->socket, NULL);
                g_object_unref (priv->socket);
                priv->socket = NULL;
        }

        G_OBJECT_CLASS (gssdp_socket_source_parent_class)->dispose (object);
}

static void
gssdp_socket_source_finalize (GObject *object)
{
        GSSDPSocketSource *self;
        GSSDPSocketSourcePrivate *priv;

        self = GSSDP_SOCKET_SOURCE (object);
        priv = gssdp_socket_source_get_instance_private (self);

        g_clear_object (&priv->address);

        if (priv->device_name != NULL) {
                g_free (priv->device_name);
                priv->device_name = NULL;
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

        g_object_class_install_property
                (object_class,
                 PROP_TYPE,
                 g_param_spec_int
                        ("type",
                         "Type",
                         "Type of socket-source (Multicast/Unicast)",
                         GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                         GSSDP_SOCKET_SOURCE_TYPE_SEARCH,
                         GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                         G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_ADDRESS,
                 g_param_spec_object
                        ("address",
                         "Host address",
                         "IP address of associated network interface",
                         G_TYPE_INET_ADDRESS,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                         G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_IFA_NAME,
                 g_param_spec_string
                        ("device-name",
                         "Interface name",
                         "Name of associated network interface",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                         G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_TTL,
                 g_param_spec_uint
                        ("ttl",
                         "TTL",
                         "Time To Live for the socket",
                         0, 255,
                         0,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                         G_PARAM_STATIC_BLURB));

        g_object_class_install_property (
                object_class,
                PROP_PORT,
                g_param_spec_uint ("port",
                                   "UDP port",
                                   "UDP port to use for TYPE_SEARCH sockets",
                                   0,
                                   G_MAXUINT16,
                                   0,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                           G_PARAM_STATIC_STRINGS));

        g_object_class_install_property
                (object_class,
                 PROP_IFA_IDX,
                 g_param_spec_int
                        ("index",
                         "Interface index",
                         "Interface index of the network device",
                         -1, G_MAXINT,
                         -1,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));
}
