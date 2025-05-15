/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
 *         Jens Georg <jensg@openismus.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define G_LOG_DOMAIN "gssdp-client"

#include <config.h>

#include <gio/gio.h>

#include "gssdp-client.h"
#include "gssdp-client-private.h"
#include "gssdp-enums.h"
#include "gssdp-error.h"
#include "gssdp-socket-source.h"
#include "gssdp-protocol.h"
#include "gssdp-net.h"
#include "gssdp-socket-functions.h"
#ifdef HAVE_PKTINFO
#include "gssdp-pktinfo-message.h"
#include "gssdp-pktinfo6-message.h"
#endif

#include <sys/types.h>
#include <glib.h>
#ifdef G_OS_WIN32
#include <winsock2.h>
#else
#include <sys/utsname.h>
#include <arpa/inet.h>
#endif

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <libsoup/soup-headers.h>

/* Size of the buffer used for reading from the socket */
#define BUF_SIZE 65536

/* interface index for loopback device */
#define LOOPBACK_IFINDEX 1

static GInetAddress *SSDP_V6_LL_ADDR = NULL;
static GInetAddress *SSDP_V6_SL_ADDR = NULL;
static GInetAddress *SSDP_V6_GL_ADDR = NULL;
static GInetAddress *SSDP_V4_ADDR = NULL;

static void
gssdp_client_initable_iface_init (gpointer g_iface,
                                  gpointer iface_data);

struct _GSSDPClientPrivate {
        char              *server_id;
        GSSDPUDAVersion    uda_version;

        GHashTable        *user_agent_cache;
        guint              socket_ttl;
        guint              msearch_port;
        GSSDPNetworkDevice device;
        GList             *headers;

        GSSDPSocketSource *request_socket;
        GSSDPSocketSource *multicast_socket;
        GSSDPSocketSource *search_socket;

        gboolean           active;
        gboolean           initialized;
        gint32             boot_id; /* "Non-negative 31 bit integer */
        gint32             config_id; /* "Non-negative 31 bit integer, User-assignable from 0 - 2^24 -1 */
};

typedef struct _GSSDPClientPrivate GSSDPClientPrivate;

/**
 * GSSDPClient:
 *
 * A simple SSDP bus handler.
 *
 * The [class@GSSDP.Client] will usually be used by the [class@GSSDP.ResourceGroup]
 * for announcing or the [class@GSSDP.ResourceBrowser] for finding resources on the network.
 *
 * A GSSDPClient is required per IP address that you want to use, even if those
 * belong t the same network device.
 */
G_DEFINE_TYPE_EXTENDED (GSSDPClient,
                        gssdp_client,
                        G_TYPE_OBJECT,
                        0,
                        G_ADD_PRIVATE(GSSDPClient)
                        G_IMPLEMENT_INTERFACE
                                (G_TYPE_INITABLE,
                                 gssdp_client_initable_iface_init))

struct _GSSDPHeaderField {
        char *name;
        char *value;
};
typedef struct _GSSDPHeaderField GSSDPHeaderField;

enum
{
        PROP_0,
        PROP_SERVER_ID,
        PROP_IFACE,
        PROP_NETWORK,
        PROP_HOST_IP,
        PROP_HOST_MASK,
        PROP_ACTIVE,
        PROP_SOCKET_TTL,
        PROP_MSEARCH_PORT,
        PROP_ADDRESS_FAMILY,
        PROP_UDA_VERSION,
        PROP_BOOT_ID,
        PROP_CONFIG_ID,
        PROP_PORT,
        PROP_HOST_ADDR,
};

enum {
        MESSAGE_RECEIVED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static const char *GSSDP_UDA_VERSION_STRINGS[] = {
        [GSSDP_UDA_VERSION_1_0] = "1.0",
        [GSSDP_UDA_VERSION_1_1] = "1.1"
};

static char *
make_server_id                (GSSDPUDAVersion uda_version);
static gboolean
request_socket_source_cb      (GIOChannel   *source,
                               GIOCondition  condition,
                               gpointer      user_data);
static gboolean
multicast_socket_source_cb    (GIOChannel   *source,
                               GIOCondition  condition,
                               gpointer      user_data);
static gboolean
search_socket_source_cb       (GIOChannel   *source,
                               GIOCondition  condition,
                               gpointer      user_data);

static gboolean
init_network_info             (GSSDPClient  *client,
                               GError      **error);

static gboolean
gssdp_client_initable_init    (GInitable     *initable,
                               GCancellable  *cancellable,
                               GError       **error);

static void
gssdp_client_init (GSSDPClient *client)
{
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        priv->active = TRUE;

}

static void
gssdp_client_initable_iface_init (gpointer               g_iface,
                                  G_GNUC_UNUSED gpointer iface_data)
{
        GInitableIface *iface = (GInitableIface *)g_iface;
        iface->init = gssdp_client_initable_init;
}

static gboolean
gssdp_client_initable_init (GInitable                   *initable,
                            G_GNUC_UNUSED GCancellable  *cancellable,
                            GError                     **error)
{
        GSSDPClient *client = GSSDP_CLIENT (initable);
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);
        GError *internal_error = NULL;

        if (priv->initialized)
                return TRUE;

        /* The following two fall-backs are there to make things
         * compatible with GSSDP 1.0.x and earlier */

        /* Fall-back to UDA 1.0 if no version is specified */
        if (priv->uda_version == GSSDP_UDA_VERSION_UNSPECIFIED) {
                priv->uda_version = GSSDP_UDA_VERSION_1_0;
        }

        /* Fall-back to IPv4 if no socket family was specified */
        if (priv->device.address_family == G_SOCKET_FAMILY_INVALID) {
                priv->device.address_family = G_SOCKET_FAMILY_IPV4;
        }

        /* Generate default server ID */
        if (priv->server_id == NULL) {
                GSSDPUDAVersion version;

                version = gssdp_client_get_uda_version (client);
                priv->server_id = make_server_id (version);
        }

        if (!gssdp_net_init (error))
                return FALSE;

        /* Make sure all network info is available to us */
        if (!init_network_info (client, &internal_error))
                goto errors;

        /* Set up sockets (Will set errno if it failed) */
        priv->request_socket =
                gssdp_socket_source_new (GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                                         priv->device.host_addr,
                                         priv->socket_ttl,
                                         priv->device.iface_name,
                                         priv->device.index,
                                         &internal_error);
        if (priv->request_socket == NULL) {
                goto errors;
        }

        gssdp_socket_source_set_callback
                        (priv->request_socket,
                        (GSourceFunc) request_socket_source_cb,
                        client);

        priv->multicast_socket =
                gssdp_socket_source_new (GSSDP_SOCKET_SOURCE_TYPE_MULTICAST,
                                         priv->device.host_addr,
                                         priv->socket_ttl,
                                         priv->device.iface_name,
                                         priv->device.index,
                                         &internal_error);
        if (priv->multicast_socket == NULL) {
            goto errors;
        }

        gssdp_socket_source_set_callback
                        (priv->multicast_socket,
                         (GSourceFunc) multicast_socket_source_cb,
                         client);

        /* Setup send socket. For security reasons, it is not recommended to
         * send M-SEARCH with source port == SSDP_PORT */
        priv->search_socket = GSSDP_SOCKET_SOURCE (g_initable_new
                                        (GSSDP_TYPE_SOCKET_SOURCE,
                                         NULL,
                                         &internal_error,
                                         "type", GSSDP_SOCKET_SOURCE_TYPE_SEARCH,
                                         "address", priv->device.host_addr,
                                         "ttl", priv->socket_ttl,
                                         "port", priv->msearch_port,
                                         "device-name", priv->device.iface_name,
                                         "index", priv->device.index,
                                         NULL));

        if (priv->search_socket != NULL) {
                if (priv->msearch_port == 0) {
                        g_object_get (priv->search_socket,
                                      "port",
                                      &priv->msearch_port,
                                      NULL);
                }

                gssdp_socket_source_set_callback
                                        (priv->search_socket,
                                         (GSourceFunc) search_socket_source_cb,
                                         client);
        }

 errors:
        if (!priv->request_socket ||
            !priv->multicast_socket ||
            !priv->search_socket) {
                g_propagate_error (error, internal_error);

                g_clear_object (&priv->request_socket);
                g_clear_object (&priv->multicast_socket);
                g_clear_object (&priv->search_socket);

                return FALSE;
        }

        gssdp_socket_source_attach (priv->request_socket);
        gssdp_socket_source_attach (priv->multicast_socket);
        gssdp_socket_source_attach (priv->search_socket);

        priv->initialized = TRUE;

        priv->user_agent_cache = g_hash_table_new_full (g_str_hash,
                                                        g_str_equal,
                                                        g_free,
                                                        g_free);

        return TRUE;
}

static void
gssdp_client_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
        GSSDPClient *client = GSSDP_CLIENT (object);
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        switch (property_id) {
        case PROP_SERVER_ID:
                g_value_set_string
                        (value,
                         gssdp_client_get_server_id (client));
                break;
        case PROP_IFACE:
                g_value_set_string (value,
                                    gssdp_client_get_interface (client));
                break;
        case PROP_NETWORK:
                g_value_set_string (value,
                                    gssdp_client_get_network (client));
                break;
        case PROP_HOST_IP:
                g_value_set_string (value,
                                    gssdp_client_get_host_ip (client));
                break;
        case PROP_HOST_ADDR:
                g_value_set_object (value, gssdp_client_get_address (client));
                break;
        case PROP_ACTIVE:
                g_value_set_boolean (value, priv->active);
                break;
        case PROP_SOCKET_TTL:
                g_value_set_uint (value, priv->socket_ttl);
                break;
        case PROP_MSEARCH_PORT:
        case PROP_PORT:
                g_value_set_uint (value, priv->msearch_port);
                break;
        case PROP_ADDRESS_FAMILY:
                g_value_set_enum (value, priv->device.address_family);
                break;
        case PROP_UDA_VERSION:
                g_value_set_enum (value, priv->uda_version);
                break;
        case PROP_BOOT_ID:
                g_value_set_int (value, priv->boot_id);
                break;
        case PROP_CONFIG_ID:
                g_value_set_int (value, priv->config_id);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_client_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
        GSSDPClient *client = GSSDP_CLIENT (object);
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        switch (property_id) {
        case PROP_SERVER_ID:
                gssdp_client_set_server_id (client,
                                            g_value_get_string (value));
                break;
        case PROP_IFACE:
                priv->device.iface_name = g_value_dup_string (value);
                break;
        case PROP_NETWORK:
                priv->device.network = g_value_dup_string (value);
                break;
        case PROP_HOST_IP:
                priv->device.host_ip = g_value_dup_string (value);
                break;
        case PROP_HOST_ADDR:
                priv->device.host_addr = g_value_dup_object (value);
                break;
        case PROP_HOST_MASK:
                priv->device.host_mask = g_value_dup_object (value);
                break;
        case PROP_ACTIVE:
                priv->active = g_value_get_boolean (value);
                break;
        case PROP_SOCKET_TTL:
                priv->socket_ttl = g_value_get_uint (value);
                break;
        case PROP_MSEARCH_PORT:
        case PROP_PORT:
                priv->msearch_port = g_value_get_uint (value);
                break;
        case PROP_ADDRESS_FAMILY:
                priv->device.address_family = g_value_get_enum (value);
                break;
        case PROP_UDA_VERSION:
                priv->uda_version = g_value_get_enum (value);
                break;
        case PROP_BOOT_ID:
                gssdp_client_set_boot_id (client, g_value_get_int (value));
                break;
        case PROP_CONFIG_ID:
                gssdp_client_set_config_id (client, g_value_get_int (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_client_dispose (GObject *object)
{
        GSSDPClient *client = GSSDP_CLIENT (object);
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        /* Destroy the SocketSources */
        g_clear_object (&priv->request_socket);
        g_clear_object (&priv->multicast_socket);
        g_clear_object (&priv->search_socket);
        g_clear_object (&priv->device.host_addr);
        g_clear_object (&priv->device.host_mask);

        G_OBJECT_CLASS (gssdp_client_parent_class)->dispose (object);
}

static void
gssdp_client_finalize (GObject *object)
{
        GSSDPClient *client = GSSDP_CLIENT (object);
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        gssdp_net_shutdown ();

        g_clear_pointer (&priv->server_id, g_free);
        g_clear_pointer (&priv->device.iface_name, g_free);
        g_clear_pointer (&priv->device.host_ip, g_free);
        g_clear_pointer (&priv->device.network, g_free);

        g_clear_pointer (&priv->user_agent_cache, g_hash_table_unref);

        G_OBJECT_CLASS (gssdp_client_parent_class)->finalize (object);
}

static void
gssdp_client_class_init (GSSDPClientClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gssdp_client_set_property;
        object_class->get_property = gssdp_client_get_property;
        object_class->dispose      = gssdp_client_dispose;
        object_class->finalize     = gssdp_client_finalize;

        /**
         * GSSDPClient:server-id:(attributes org.gtk.Property.get=gssdp_client_get_server_id org.gtk.Property.set=gssdp_client_set_server_id):
         *
         * The SSDP server's identifier.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SERVER_ID,
                 g_param_spec_string
                         ("server-id",
                          "Server ID",
                          "The SSDP server's identifier.",
                          NULL,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPClient:interface:(attributes org.gtk.Property.get=gssdp_client_get_interface):
         *
         * The name of the network interface this client is associated with.
         * Set to NULL to autodetect.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_IFACE,
                 g_param_spec_string
                         ("interface",
                          "Network interface",
                          "The name of the associated network interface.",
                          NULL,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:network:(attributes org.gtk.Property.get=gssdp_client_get_network):
         *
         * The network this client is currently connected to. You could set this
         * to anything you want to identify the network this client is
         * associated with. If you are using #GUPnPContextManager and associated
         * interface is a WiFi interface, this property is set to the ESSID of
         * the network. Otherwise, expect this to be the network IP address by
         * default.
         **/
        g_object_class_install_property (
                object_class,
                PROP_NETWORK,
                g_param_spec_string (
                        "network",
                        "Network ID",
                        "The network this client is currently connected to.",
                        NULL,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:host-ip:(attributes org.gtk.Property.get=gssdp_client_get_host_ip):
         *
         * The IP address of the assoicated network interface.
         *
         * Deprecated: 1.6.0: Use [property@GSSDP.Client:address] instead.
         **/
        g_object_class_install_property (
                object_class,
                PROP_HOST_IP,
                g_param_spec_string ("host-ip",
                                     "Host IP",
                                     "The IP address of the associated"
                                     "network interface",
                                     NULL,
                                     G_PARAM_READWRITE |
                                             G_PARAM_CONSTRUCT_ONLY |
                                             G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:address:(attributes org.gtk.Property.get=gssdp_client_get_address):
         *
         * The network address this client is bound to.
         * Since: 1.6
         **/
        g_object_class_install_property (
                object_class,
                PROP_HOST_ADDR,
                g_param_spec_object ("address",
                                     "Network address",
                                     "The internet address of the client",
                                     G_TYPE_INET_ADDRESS,
                                     G_PARAM_READWRITE |
                                             G_PARAM_CONSTRUCT_ONLY |
                                             G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:host-mask:(attributes org.gtk.Property.get=gssdp_client_get_address_mask):
         *
         * The network mask of the assoicated network interface.
         **/
        g_object_class_install_property (
                object_class,
                PROP_HOST_MASK,
                g_param_spec_object ("host-mask",
                                     "Host network mask",
                                     "The IP netmask of the associated"
                                     "network interface",
                                     G_TYPE_INET_ADDRESS_MASK,
                                     G_PARAM_READWRITE |
                                             G_PARAM_CONSTRUCT_ONLY |
                                             G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:active:(attributes org.gtk.Property.get=gssdp_client_get_active):
         *
         * Whether this client is active or not (passive). When active
         * (default), the client sends messages on the network, otherwise
         * not. In most cases, you don't want to touch this property.
         *
         **/
        g_object_class_install_property (
                object_class,
                PROP_ACTIVE,
                g_param_spec_boolean ("active",
                                      "Active",
                                      "TRUE if the client is active.",
                                      TRUE,
                                      G_PARAM_READWRITE |
                                              G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:socket-ttl:
         *
         * Time-to-live value to use for all sockets created by this client.
         * If not set (or set to 0) the value recommended by UPnP will be used.
         * This property can only be set during object construction.
         */
        g_object_class_install_property (
                object_class,
                PROP_SOCKET_TTL,
                g_param_spec_uint ("socket-ttl",
                                   "Socket TTL",
                                   "Time To Live for client's sockets",
                                   0,
                                   255,
                                   0,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                           G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:msearch-port:
         *
         * UDP port to use for sending multicast M-SEARCH requests on the
         * network. If not set (or set to 0) a random port will be used.
         * This property can be only set during object construction.
         *
         * Deprecated: 1.6.0: Use [property@GSSDP.Client:port] instead
         */
        g_object_class_install_property (
                object_class,
                PROP_MSEARCH_PORT,
                g_param_spec_uint ("msearch-port",
                                   "M-SEARCH port",
                                   "UDP port to use for M-SEARCH requests",
                                   0,
                                   G_MAXUINT16,
                                   0,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                           G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:port:
         *
         * UDP port to use for sending multicast M-SEARCH requests on the
         * network. If not set (or set to 0) a random port will be used.
         * This property can be only set during object construction.
         *
         * Since: 1.6.0
         */
        g_object_class_install_property (
                object_class,
                PROP_PORT,
                g_param_spec_uint ("port",
                                   "M-SEARCH port",
                                   "UDP port to use for M-SEARCH requests",
                                   0,
                                   G_MAXUINT16,
                                   0,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                           G_PARAM_STATIC_STRINGS));
        /**
         * GSSDPClient:address-family:(attributes org.gtk.Property.get=gssdp_client_get_family):
         *
         * The IP protocol address family this client works on. When specified
         * during construction without giving a concrete address, it will be
         * used to determine the proper address.
         *
         * If not specified, will contain the currrent address family after
         * the call to [method@Glib.Initable.init]. Use %G_SOCKET_FAMILY_INVALID
         * to specifiy using the default socket family (legacy IP)
         *
         * Since: 1.2.0
         */
        g_object_class_install_property (
                object_class,
                PROP_ADDRESS_FAMILY,
                g_param_spec_enum (
                        "address-family",
                        "IP Address family",
                        "IP address family to prefer when creating the client",
                        G_TYPE_SOCKET_FAMILY,
                        G_SOCKET_FAMILY_INVALID,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:uda-version:(attributes org.gtk.Property.get=gssdp_client_get_uda_version):
         *
         * The UPnP version the client adheres to.
         *
         * Since: 1.2.0
         */
        g_object_class_install_property (
                object_class,
                PROP_UDA_VERSION,
                g_param_spec_enum (
                        "uda-version",
                        "UDA version",
                        "UPnP Device Architecture version on this client",
                        GSSDP_TYPE_UDA_VERSION,
                        GSSDP_UDA_VERSION_1_0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:boot-id:(attributes org.gtk.Property.set=gssdp_client_set_boot_id):
         *
         * The value of the BOOTID.UPNP.ORG header
         *
         * Since 1.2.0
         */
        g_object_class_install_property (
                object_class,
                PROP_BOOT_ID,
                g_param_spec_int ("boot-id",
                                  "current boot-id value",
                                  "Value of the BOOTID.UPNP.ORG header",
                                  -1,
                                  G_MAXINT32,
                                  -1,
                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                          G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient:config-id:(attributes org.gtk.Property.set=gssdp_client_set_config_id):
         *
         * The value of the CONFIGID.UPNP.ORG header
         *
         * Since 1.2.0
         */
        g_object_class_install_property (
                object_class,
                PROP_CONFIG_ID,
                g_param_spec_int ("config-id",
                                  "current config-id value",
                                  "Value of the CONFIGID.UPNP.ORG header",
                                  -1,
                                  G_MAXINT32,
                                  -1,
                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                          G_PARAM_STATIC_STRINGS));

        /**
         * GSSDPClient::message-received: (skip)
         * @client: The #GSSDPClient that received the message.
         * @from_ip: The IP address of the source.
         * @from_port: The UDP port used by the sender.
         * @type: The #_GSSDPMessageType.
         * @headers: (type SoupMessageHeaders): Parsed #SoupMessageHeaders from the message.
         *
         * Internal signal.
         *
         * Stability: Private
         **/
        signals[MESSAGE_RECEIVED] =
                g_signal_new ("message-received",
                              GSSDP_TYPE_CLIENT,
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL, NULL,
                              G_TYPE_NONE,
                              4,
                              G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                              G_TYPE_UINT,
                              G_TYPE_INT,
                              soup_message_headers_get_type());
}


/**
 * gssdp_client_new:
 * @iface: (nullable): The name of the network interface, or %NULL for
 * auto-detection.
 * @error: (nullable): Location to store error, or %NULL
 *
 * Creates a GSSDP client on @iface. GSSDPClient will pick the address it finds
 * suitable for using.
 *
 * Using this utility function, the created client will be using UDA 1.0 and
 * IPv4 only.
 *
 * Deprecated: 1.6.0: Use [ctor@GSSDP.Client.new_for_address] instead.
 *
 * Return value: (nullable): A new #GSSDPClient object.
 **/
GSSDPClient *
gssdp_client_new (const char *iface, GError **error)
{
        return g_initable_new (GSSDP_TYPE_CLIENT,
                               NULL,
                               error,
                               "interface", iface,
                               NULL);
}

/**
 * gssdp_client_new_with_port:
 * @iface: (nullable): The name of the network interface, or %NULL for
 * auto-detection.
 * @msearch_port: The network port to use for M-SEARCH requests or 0 for
 * random.
 * @error: (allow-none): Location to store error, or %NULL.
 *
 * Creates a GSSDP client on @iface. GSSDPClient will pick the address it finds
 * suitable for using.
 *
 * Using this utility function, the created client will be using UDA 1.0 and IPv4 only.
 *
 * Deprecated: 1.6.0: Use [ctor@GSSDP.Client.new_for_address] instead.
 *
 * Return value: (nullable):  A new #GSSDPClient object or %NULL on error.
 */
GSSDPClient *
gssdp_client_new_with_port (const char *iface,
                            guint16     msearch_port,
                            GError    **error)
{
        return g_initable_new (GSSDP_TYPE_CLIENT,
                               NULL,
                               error,
                               "interface",
                               iface,
                               "port",
                               msearch_port,
                               NULL);
}

/**
 * gssdp_client_new_full:
 * @iface: (nullable): the name of a network interface
 * @addr: (nullable): an IP address or %NULL for auto-detection. If you do not
 * care about the address, but want to specify an address family, use
 * [ctor@Glib.InetAddress.new_any] with the appropriate family instead.
 * @port: The network port to use for M-SEARCH requests or 0 for
 * random.
 * @uda_version: The UDA version this client will adhere to
 * @error: (allow-none): Location to store error, or %NULL.
 *
 * Creates a GSSDP client with address @addr. If none is specified, GSSDP
 * will chose the address it deems most suitable.
 *
 * Since: 1.6.0
 *
 * Return value: (nullable):  A new #GSSDPClient object or %NULL on error.
 */
GSSDPClient *
gssdp_client_new_full (const char *iface,
                       GInetAddress *addr,
                       guint16 port,
                       GSSDPUDAVersion uda_version,
                       GError **error)
{
        return g_initable_new (GSSDP_TYPE_CLIENT,
                               NULL,
                               error,
                               "interface",
                               iface,
                               "address",
                               addr,
                               "port",
                               port,
                               "uda-version",
                               uda_version,
                               NULL);
}

/**
 * gssdp_client_new_for_address
 * @addr: (nullable): an IP address or %NULL for auto-detection. If you do not
 * care about the address, but want to specify an address family, use
 * [ctor@Glib.InetAddress.new_any] with the appropriate family instead.
 * @port: The network port to use for M-SEARCH requests or 0 for
 * random.
 * @uda_version: The UDA version this client will adhere to
 * @error: (allow-none): Location to store error, or %NULL.
 *
 * Creates a GSSDP client with address @addr. If none is specified, GSSDP
 * will chose the address it deems most suitable.
 *
 * Since: 1.6.0
 *
 * Return value: (nullable):  A new #GSSDPClient object or %NULL on error.
 */
GSSDPClient *
gssdp_client_new_for_address (GInetAddress *addr,
                              guint16 port,
                              GSSDPUDAVersion uda_version,
                              GError **error)
{
        return gssdp_client_new_full (NULL, addr, port, uda_version, error);
}

/**
 * gssdp_client_set_server_id:(attributes org.gtk.Method.set_property=server-id):
 * @client: A #GSSDPClient
 * @server_id: The server ID
 *
 * Sets the server ID of @client to @server_id. This string is used as the
 * "Server:" identification header for SSDP discovery and response packets
 * and "User-Agent" header for searches.
 *
 * By default, GSSDP will generate a header conforming to the requirements
 * defined in the UDA documents: OS/Version UPnP/Version GSSDP/Version.
 */
void
gssdp_client_set_server_id (GSSDPClient *client,
                            const char  *server_id)
{
        GSSDPClientPrivate *priv = NULL;

        g_return_if_fail (GSSDP_IS_CLIENT (client));
        priv = gssdp_client_get_instance_private (client);

        g_clear_pointer (&priv->server_id, g_free);

        if (server_id)
                priv->server_id = g_strdup (server_id);

        g_object_notify (G_OBJECT (client), "server-id");
}

/**
 * gssdp_client_get_server_id:(attributes org.gtk.Method.get_property=server-id):
 * @client: A #GSSDPClient
 *
 * Return value: The server ID.
 **/
const char *
gssdp_client_get_server_id (GSSDPClient *client)
{
        GSSDPClientPrivate *priv = NULL;

        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);
        priv = gssdp_client_get_instance_private (client);

        return priv->server_id;
}

/**
 * gssdp_client_get_interface:(attributes org.gtk.Method.get_property=interface):
 * @client: A #GSSDPClient
 *
 * Get the name of the network interface associated to @client.
 *
 * Return value: The network interface name. This string should not be freed.
 **/
const char *
gssdp_client_get_interface (GSSDPClient *client)
{
        GSSDPClientPrivate *priv = NULL;

        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);
        priv = gssdp_client_get_instance_private (client);


        return priv->device.iface_name;
}

/**
 * gssdp_client_get_host_ip:(attributes org.gtk.Method.get_property=host-ip):
 * @client: A #GSSDPClient
 *
 * Get the IP address we advertise ourselves as using.
 *
 * Return value: The IP address. This string should not be freed.
 **/
const char *
gssdp_client_get_host_ip (GSSDPClient *client)
{
        GSSDPClientPrivate *priv = NULL;

        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);
        priv = gssdp_client_get_instance_private (client);

        if (priv->device.host_ip == NULL)
                if (priv->device.host_addr != NULL)
                        priv->device.host_ip = g_inet_address_to_string
                                            (priv->device.host_addr);

        return priv->device.host_ip;
}

/**
 * gssdp_client_set_network:(attributes org.gtk.Method.set_property=network):
 * @client: A #GSSDPClient
 * @network: The string identifying the network
 *
 * Sets the network identification of @client to @network.
 **/
void
gssdp_client_set_network (GSSDPClient *client,
                          const char  *network)
{
        GSSDPClientPrivate *priv = NULL;

        g_return_if_fail (GSSDP_IS_CLIENT (client));
        priv = gssdp_client_get_instance_private (client);

        g_clear_pointer (&priv->device.network, g_free);

        if (network)
                priv->device.network = g_strdup (network);

        g_object_notify (G_OBJECT (client), "network");
}

/**
 * gssdp_client_add_cache_entry:
 * @client: A #GSSDPClient
 * @ip_address: The host to add to the cache
 * @user_agent: User agent ot the host to add
 *
 * Add @user_agent for @ip_address.
 *
 * Each [class@GSSDP.Client] maintains a mapping of addresses
 * (MAC on systems that support it, IP addresses otherwise) to User Agents.
 *
 * This information can be used in higher layers to get an User-Agent for
 * devices that do not set the User-Agent header in their SOAP requests.
 *
 **/
void
gssdp_client_add_cache_entry (GSSDPClient  *client,
                               const char   *ip_address,
                               const char   *user_agent)
{
        GSSDPClientPrivate *priv = NULL;
        char *hwaddr = NULL;

        g_return_if_fail (GSSDP_IS_CLIENT (client));
        g_return_if_fail (ip_address != NULL);
        g_return_if_fail (user_agent != NULL);

        priv = gssdp_client_get_instance_private (client);

        hwaddr = gssdp_net_mac_lookup (&priv->device, ip_address);

        if (hwaddr)
                g_hash_table_insert (priv->user_agent_cache,
                                     hwaddr,
                                     g_strdup (user_agent));
}

/**
 * gssdp_client_guess_user_agent:
 * @client: A #GSSDPClient
 * @ip_address: IP address to guess the user-agent for
 *
 * Try to get a User-Agent for @ip_address.
 *
 * Returns: (transfer none): The User-Agent cached for this IP, %NULL if none
 * is cached.
 **/
const char *
gssdp_client_guess_user_agent (GSSDPClient *client,
                               const char  *ip_address)
{
        GSSDPClientPrivate *priv = NULL;
        char *hwaddr = NULL;

        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);
        g_return_val_if_fail (ip_address != NULL, NULL);

        priv = gssdp_client_get_instance_private (client);

        hwaddr = gssdp_net_mac_lookup (&priv->device, ip_address);

        if (hwaddr) {
                const char *agent;

                agent =  g_hash_table_lookup (priv->user_agent_cache,
                                              hwaddr);
                g_free (hwaddr);

                return agent;
        }

        return NULL;
}

/**
 * gssdp_client_get_network:(attributes org.gtk.Method.get_property=network):
 * @client: A [class@GSSDP.Client]
 *
 * Get the network identifier of the client. See [property@GSSDP.Client:network]
 * for  details.
 *
 * Return value: The network identification. This string should not be freed.
 */
const char *
gssdp_client_get_network (GSSDPClient *client)
{
        GSSDPClientPrivate *priv = NULL;
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);

        priv = gssdp_client_get_instance_private (client);

        return priv->device.network;
}

/**
 * gssdp_client_get_active:(attributes org.gtk.Method.get_property=active):
 * @client: A #GSSDPClient
 *
 * Get the current state of the client. See [property@GSSDP.Client:active] for details.
 *
 * Return value: %TRUE if @client is active, %FALSE otherwise.
 **/
gboolean
gssdp_client_get_active (GSSDPClient *client)
{
        GSSDPClientPrivate *priv = NULL;
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), FALSE);

        priv = gssdp_client_get_instance_private (client);

        return priv->active;
}

static void
header_field_free (GSSDPHeaderField *header)
{
        g_free (header->name);
        g_free (header->value);
        g_slice_free (GSSDPHeaderField, header);
}

static gchar *
append_header_fields (GList *headers, const gchar *message)
{
        GString *str = NULL;
        GList *iter = NULL;

        str = g_string_new (message);

        for (iter = headers; iter; iter = iter->next) {
                GSSDPHeaderField *header = (GSSDPHeaderField *) iter->data;
                g_string_append_printf (str, "%s: %s\r\n",
                                        header->name,
                                        header->value ? header->value : "");
        }

        g_string_append (str, "\r\n");

        return g_string_free (str, FALSE);
}

/**
 * gssdp_client_append_header:
 * @client: A #GSSDPClient
 * @name: Header name
 * @value: (allow-none): Header value
 *
 * Adds a header field to the messages sent by this @client. It is intended to
 * be used by clients requiring vendor specific header fields.
 *
 * If there is an existing header with @name it will append another one.
 **/
void
gssdp_client_append_header (GSSDPClient *client,
                            const char  *name,
                            const char  *value)
{
        GSSDPHeaderField *header = NULL;
        GSSDPClientPrivate *priv = NULL;

        g_return_if_fail (GSSDP_IS_CLIENT (client));
        g_return_if_fail (name != NULL);
        g_return_if_fail (value != NULL);

        priv = gssdp_client_get_instance_private (client);

        header = g_slice_new (GSSDPHeaderField);
        header->name = g_strdup (name);
        header->value = g_strdup (value);
        priv->headers = g_list_append (priv->headers, header);
}

/**
 * gssdp_client_remove_header:
 * @client: A #GSSDPClient
 * @name: Header name
 *
 * Removes @name from the list of headers. If there are multiple values for
 * @name, they are all removed.
 **/
void
gssdp_client_remove_header (GSSDPClient *client,
                            const char  *name)
{
        GSSDPClientPrivate *priv;
        GList *l;

        g_return_if_fail (GSSDP_IS_CLIENT (client));
        g_return_if_fail (name != NULL);

        priv = gssdp_client_get_instance_private (client);
        l = priv->headers;
        while (l != NULL) {
                GList *next = l->next;
                GSSDPHeaderField *header = l->data;

                if (!g_strcmp0 (header->name, name)) {
                        header_field_free (header);
                        priv->headers = g_list_delete_link (priv->headers, l);
                }
                l = next;
        }
}

/**
 * gssdp_client_clear_headers:
 * @client: A #GSSDPClient
 *
 * Removes all the headers for this @client.
 **/
void
gssdp_client_clear_headers (GSSDPClient *client)
{
        GSSDPClientPrivate *priv = NULL;
        GList *l;

        g_return_if_fail (GSSDP_IS_CLIENT (client));
        priv = gssdp_client_get_instance_private (client);

        l = priv->headers;
        while (l != NULL)
        {
                GList *next = l->next;
                GSSDPHeaderField *header = l->data;

                if (g_strcmp0 (header->name, "BOOTID.UPNP.ORG") != 0 &&
                    g_strcmp0 (header->name, "CONFIGID.UPNP.ORG") != 0) {
                        header_field_free (header);
                        priv->headers = g_list_delete_link (priv->headers, l);
                }
                l = next;
        }
}

/**
 * gssdp_client_get_address:(attributes org.gtk.Method.get_property=address):
 * @client: A #GSSDPClient
 *
 * The IP address this client works on.
 *
 * Returns: (transfer full): The #GInetAddress this client works on
 **/
GInetAddress *
gssdp_client_get_address (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        return g_object_ref (priv->device.host_addr);
}

/**
 * gssdp_client_get_index:
 * @client: A #GSSDPClient
 *
 * Returns: The interface index of this client
 **/
guint
gssdp_client_get_index (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), 0);
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        return priv->device.index;
}

/**
 * gssdp_client_get_family:(attributes org.gtk.Method.get_property=address-family):
 * @client: A #GSSDPClient
 *
 * Returns: IP protocol version (%G_SOCKET_FAMILY_IPV4 or G_SOCKET_FAMILY_IPV6)
 * this client uses
 */
GSocketFamily
gssdp_client_get_family (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), G_SOCKET_FAMILY_INVALID);
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        return g_inet_address_get_family (priv->device.host_addr);
}

/**
 * gssdp_client_get_address_mask:(attributes org.gtk.Method.get_property=host-mask):
 * @client: A #GSSDPClient
 *
 * Since: 1.2.3
 *
 * Returns: (transfer full): Address mask of this client
 */
GInetAddressMask *
gssdp_client_get_address_mask (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);

        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        return g_object_ref (priv->device.host_mask);
}

/**
 * gssdp_client_get_uda_version:(attributes org.gtk.Method.get_property=uda-version):
 * @client: A #GSSDPClient
 *
 * Returns: the UDA protocol version this client adheres to
 */
GSSDPUDAVersion
gssdp_client_get_uda_version  (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), GSSDP_UDA_VERSION_UNSPECIFIED);
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        return priv->uda_version;
}

/**
 * gssdp_client_set_boot_id:(attributes org.gtk.Method.set_property=boot-id):
 * @client: A #GSSDPClient
 * @boot_id: The new boot-id for the client
 *
 * Will set the new boot-id for this SSDP client. Does nothing if the UDA
 * version used by the client is UDA 1.0
 *
 * The boot-id is used to signalize changes in the network configuration
 * for multi-homed hosts
 */
void
gssdp_client_set_boot_id (GSSDPClient *client, gint32 boot_id)
{

        g_return_if_fail (GSSDP_IS_CLIENT (client));

        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        priv->boot_id = boot_id;

        if (priv->uda_version > GSSDP_UDA_VERSION_1_0) {
                char *id_string;
                gssdp_client_remove_header (client, "BOOTID.UPNP.ORG");

                id_string = g_strdup_printf ("%d", boot_id);
                gssdp_client_append_header (client, "BOOTID.UPNP.ORG", id_string);
                g_free (id_string);
        }

}

/**
 * gssdp_client_set_config_id:(attributes org.gtk.Method.set_property=config-id):
 * @client: A #GSSDPClient
 * @config_id: The new config-id for the client
 *
 * The config-id is used to allow caching of the device or service description.
 * It should be changed if that changes.
 */
void
gssdp_client_set_config_id (GSSDPClient *client, gint32 config_id)
{
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        if (priv->uda_version > GSSDP_UDA_VERSION_1_0) {
                char *id_string;
                priv->config_id = config_id;
                gssdp_client_remove_header (client, "CONFIGID.UPNP.ORG");

                id_string = g_strdup_printf ("%d", config_id);
                gssdp_client_append_header (client, "CONFIGID.UPNP.ORG", id_string);
                g_free (id_string);
        }

}

/**
 * gssdp_client_can_reach:
 * @client: A #GSSDPClient
 * @address: A #GInetSocketAddress of the target. The port part of the address may be 0
 *
 * Check if the peer at @address is reachable using this @client.
 *
 * Since: 1.2.4
 * Returns: %TRUE if considered reachable, %FALSE otherwise.
 */
gboolean
gssdp_client_can_reach (GSSDPClient *client, GInetSocketAddress *address)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), FALSE);
        g_return_val_if_fail (G_IS_INET_SOCKET_ADDRESS (address), FALSE);

        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        GInetAddress *addr = g_inet_socket_address_get_address (address);
        if (g_inet_address_get_is_link_local (addr) &&
            g_inet_address_get_family (addr) == G_SOCKET_FAMILY_IPV6) {
                return g_inet_socket_address_get_scope_id (address) ==
                       priv->device.index;
        }

        return g_inet_address_mask_matches (priv->device.host_mask, addr);
}

guint
gssdp_client_get_port (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), 0);

        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        return priv->msearch_port;
}

/**
 * _gssdp_client_send_message:
 * @client: A #GSSDPClient
 * @dest_ip: (allow-none): The destination IP address, or %NULL to broadcast
 * @dest_port: (allow-none): The destination port, or %NULL for default
 * @message: The message to send
 *
 * Sends @message to @dest_ip.
 **/
void
_gssdp_client_send_message (GSSDPClient      *client,
                            const char       *dest_ip,
                            gushort           dest_port,
                            const char       *message,
                            _GSSDPMessageType type)
{
        GSSDPClientPrivate *priv = NULL;
        gssize res;
        GError *error = NULL;
        GInetAddress *inet_address = NULL;
        GSocketAddress *address = NULL;
        GSocket *socket;
        char *extended_message;

        g_return_if_fail (GSSDP_IS_CLIENT (client));
        g_return_if_fail (message != NULL);

        priv = gssdp_client_get_instance_private (client);

        g_return_if_fail (priv->initialized);

        if (!priv->active)
                /* We don't send messages in passive mode */
                return;

        /* Broadcast if @dest_ip is NULL */
        if (dest_ip == NULL) {
                dest_ip = _gssdp_client_get_mcast_group (client);
        }

        /* Use default port if no port was explicitly specified */
        if (dest_port == 0)
                dest_port = SSDP_PORT;

        if (type == _GSSDP_DISCOVERY_REQUEST)
                socket = gssdp_socket_source_get_socket
                                        (priv->search_socket);
        else
                socket = gssdp_socket_source_get_socket
                                        (priv->request_socket);

        inet_address = g_inet_address_new_from_string (dest_ip);
        address = g_inet_socket_address_new (inet_address, dest_port);
        extended_message = append_header_fields (priv->headers, message);

        res = g_socket_send_to (socket,
                                address,
                                extended_message,
                                strlen (extended_message),
                                NULL,
                                &error);

        if (res == -1) {
                g_warning ("Error sending SSDP packet to %s: %s",
                           dest_ip,
                           error->message);
                g_error_free (error);
        }

        g_free (extended_message);
        g_object_unref (address);
        g_object_unref (inet_address);
}

const char*
_gssdp_client_get_mcast_group (GSSDPClient *client)
{
        GSocketFamily family;
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        family = g_inet_address_get_family (priv->device.host_addr);
        if (family == G_SOCKET_FAMILY_IPV4)
                return SSDP_ADDR;
        else {
                /* IPv6 */
                /* According to Annex.A, we need to check the scope of the
                 * address to use the proper multicast group */
                if (g_inet_address_get_is_link_local (priv->device.host_addr)) {
                            return SSDP_V6_LL;
                } else if (g_inet_address_get_is_site_local (
                                   priv->device.host_addr)) {
                        return SSDP_V6_SL;
                } else {
                        return SSDP_V6_GL;
                }
        }
}

#define ENSURE_V6_GROUP(group) \
        G_STMT_START { \
                if (SSDP_V6_ ## group ## _ADDR == NULL) { \
                        SSDP_V6_ ## group ## _ADDR = \
                                g_inet_address_new_from_string (SSDP_V6_ ## group); \
                } \
        } \
        G_STMT_END

static GInetAddress *
_gssdp_client_get_mcast_group_addr (GSSDPClient *client)
{
        GSocketFamily family;
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        family = g_inet_address_get_family (priv->device.host_addr);
        if (family == G_SOCKET_FAMILY_IPV4) {
                if (SSDP_V4_ADDR == NULL) {
                        SSDP_V4_ADDR = g_inet_address_new_from_string (SSDP_ADDR);
                }

                return SSDP_V4_ADDR;
        } else {
                /* IPv6 */
                /* According to Annex.A, we need to check the scope of the
                 * address to use the proper multicast group */
                if (g_inet_address_get_is_link_local (priv->device.host_addr)) {
                            ENSURE_V6_GROUP(LL);

                            return SSDP_V6_LL_ADDR;
                } else if (g_inet_address_get_is_site_local (
                                   priv->device.host_addr)) {
                        ENSURE_V6_GROUP (SL);

                        return SSDP_V6_SL_ADDR;
                } else {
                        ENSURE_V6_GROUP (GL);

                        return SSDP_V6_GL_ADDR;
                }
        }
}

/*
 * Generates the default server ID
 */
static char *
make_server_id (GSSDPUDAVersion uda_version)
{
#ifdef G_OS_WIN32
        OSVERSIONINFO versioninfo;
        versioninfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
        if (GetVersionEx (&versioninfo)) {
                return g_strdup_printf ("Microsoft Windows/%ld.%ld UPnP/%s GSSDP/%s",
                                        versioninfo.dwMajorVersion,
                                        versioninfo.dwMinorVersion,
                                        GSSDP_UDA_VERSION_STRINGS[uda_version],
                                        VERSION);
        } else {
                return g_strdup_printf ("Microsoft Windows/Unknown UPnP/%s GSSDP/%s",
                                        GSSDP_UDA_VERSION_STRINGS[uda_version],
                                        VERSION);
        }
#else
        struct utsname sysinfo;

        uname (&sysinfo);

        return g_strdup_printf ("%s/%s UPnP/%s GSSDP/%s",
                                sysinfo.sysname,
                                sysinfo.release,
                                GSSDP_UDA_VERSION_STRINGS[uda_version],
                                VERSION);
#endif
}

static gboolean
parse_http_request (char                *buf,
                    int                  len,
                    SoupMessageHeaders **headers,
                    int                 *type)
{
        char *req_method = NULL;
        char *path = NULL;
        SoupHTTPVersion version;

        *headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_REQUEST);

        if (soup_headers_parse_request (buf,
                                        len,
                                        *headers,
                                        &req_method,
                                        &path,
                                        &version) == SOUP_STATUS_OK &&
            version == SOUP_HTTP_1_1 &&
            (path && g_ascii_strncasecmp (path, "*", 1) == 0)) {
                if (g_ascii_strncasecmp (req_method,
                                         SSDP_SEARCH_METHOD,
                                         strlen (SSDP_SEARCH_METHOD)) == 0)
                        *type = _GSSDP_DISCOVERY_REQUEST;
                else if (g_ascii_strncasecmp (req_method,
                                              GENA_NOTIFY_METHOD,
                                              strlen (GENA_NOTIFY_METHOD)) == 0)
                        *type = _GSSDP_ANNOUNCEMENT;
                else
                        g_warning ("Unhandled method '%s'", req_method);

                g_free (req_method);
                g_free (path);

                return TRUE;
        } else {
                soup_message_headers_unref (*headers);
                *headers = NULL;

                g_free (path);
                g_free (req_method);

                return FALSE;
        }
}

static gboolean
parse_http_response (char                *buf,
                    int                  len,
                    SoupMessageHeaders **headers,
                    int                 *type)
{
        guint status_code;

        *headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);

        if (soup_headers_parse_response (buf,
                                         len,
                                         *headers,
                                         NULL,
                                         &status_code,
                                         NULL) &&
            status_code == 200) {
                *type = _GSSDP_DISCOVERY_RESPONSE;

                return TRUE;
        } else {
                g_clear_pointer (headers, soup_message_headers_unref);

                return FALSE;
        }
}

/*
 * Called when data can be read from the socket
 */
static gboolean
socket_source_cb (GSSDPSocketSource *socket_source, GSSDPClient *client)
{
        int type, len;
        char buf[BUF_SIZE], *end;
        SoupMessageHeaders *headers = NULL;
        GSocket *socket;
        GSocketAddress *address = NULL;
        gssize bytes;
        GInetAddress *inetaddr;
        char *ip_string = NULL;
        guint16 port;
        GError *error = NULL;
        GInputVector vector;
        GSocketControlMessage **messages = NULL;
        gint num_messages = 0;
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);
        gboolean ret = TRUE;

        vector.buffer = buf;
        vector.size = BUF_SIZE;

        /* Get Socket */
        socket = gssdp_socket_source_get_socket (socket_source);
        bytes = g_socket_receive_message (socket,
                                          &address,
                                          &vector,
                                          1,
                                          &messages,
                                          &num_messages,
                                          NULL,
                                          NULL,
                                          &error);

        if (bytes == -1) {
                if (!g_error_matches (error,
                                      G_IO_ERROR,
                                      G_IO_ERROR_WOULD_BLOCK)) {
                        g_warning ("Failed to receive from socket: %s",
                                   error->message);
                        ret = FALSE;
                }

                goto out;
        }

#if defined(HAVE_PKTINFO)
        {
                int i;
                for (i = 0; i < num_messages; i++) {
                        gint msg_ifindex;
                        GInetAddress *local_addr;
                        GInetAddress *group_addr;

                        group_addr = _gssdp_client_get_mcast_group_addr (client);

                        if (GSSDP_IS_PKTINFO_MESSAGE (messages[i])) {
                                GSSDPPktinfoMessage *msg;

                                msg = GSSDP_PKTINFO_MESSAGE (messages[i]);
                                msg_ifindex = gssdp_pktinfo_message_get_ifindex (msg);
                                local_addr = gssdp_pktinfo_message_get_pkt_addr (msg);
                        } else if (GSSDP_IS_PKTINFO6_MESSAGE (messages[i])) {
                                GSSDPPktinfo6Message *msg;

                                msg = GSSDP_PKTINFO6_MESSAGE (messages[i]);
                                msg_ifindex = gssdp_pktinfo6_message_get_ifindex (msg);
                                local_addr = gssdp_pktinfo6_message_get_local_addr (msg);
                        } else {
                                continue;
                        }

                        /* message needs to be on correct interface or on
                         * loopback (as kernel can be smart and route things
                         * there even if sent to another network) */
                        if (g_inet_address_equal (local_addr, group_addr)) {
                                // This is a multicast packet. If the index is not our index, ignore
                                if (msg_ifindex != priv->device.index) {
                                        goto out;
                                }
                                break;
                        }

                        if (g_inet_address_equal (local_addr,
                                                  priv->device.host_addr)) {
                                // This is a "normal" packet. We can receive those

                                if (msg_ifindex != priv->device.index &&
                                    msg_ifindex != LOOPBACK_IFINDEX) {
                                        goto out;
                                }
                                break;
                        }
                }
        }
#else
        /* We need the following lines to make sure the right client received
         * the packet. We won't need to do this if there was any way to tell
         * Mr. Unix that we are only interested in receiving multicast packets
         * on this socket from a particular interface but AFAIK that is not
         * possible, at least not in a portable way.
         */

        if (!gssdp_client_can_reach (client, G_INET_SOCKET_ADDRESS(address))) {
                goto out;
        }
#endif

        if (bytes >= BUF_SIZE) {
                g_warning ("Received packet of %" G_GSSIZE_FORMAT " bytes, "
                           "but the maximum buffer size is %d. Packed dropped.",
                           bytes, BUF_SIZE);

                goto out;
        }

        /* Add trailing \0 */
        buf[bytes] = '\0';

        /* Find length */
        end = strstr (buf, "\r\n\r\n");
        if (!end) {
                g_debug ("Received packet lacks \"\\r\\n\\r\\n\" sequence. "
                         "Packed dropped.");

                goto out;
        }

        len = end - buf + 2;

        /* Parse message */
        type = -1;
        headers = NULL;

        if (!parse_http_request (buf,
                                 len,
                                 &headers,
                                 &type)) {
                if (!parse_http_response (buf,
                                          len,
                                          &headers,
                                          &type)) {
                        g_debug ("Unhandled packet '%s'", buf);
                }
        }

        /* Emit signal if parsing succeeded */
        inetaddr = g_inet_socket_address_get_address (
                                        G_INET_SOCKET_ADDRESS (address));
        ip_string = g_inet_address_to_string (inetaddr);
        port = g_inet_socket_address_get_port (
                                        G_INET_SOCKET_ADDRESS (address));

        if (type >= 0) {
                const char *agent;

                /* update client cache */
                agent = soup_message_headers_get_one (headers, "Server");
                if (!agent)
                        agent = soup_message_headers_get_one (headers,
                                                              "User-Agent");

                if (agent)
                        gssdp_client_add_cache_entry (client,
                                                      ip_string,
                                                      agent);

                g_signal_emit (client,
                               signals[MESSAGE_RECEIVED],
                               0,
                               ip_string,
                               port,
                               type,
                               headers);
        }

out:
        g_clear_error (&error);

        g_free (ip_string);

        g_clear_pointer (&headers, soup_message_headers_unref);
        g_clear_object (&address);

        if (messages) {
                int i;
                for (i = 0; i < num_messages; i++)
                        g_object_unref (messages[i]);

                g_free (messages);
        }

        return ret;
}

static gboolean
request_socket_source_cb (G_GNUC_UNUSED GIOChannel  *source,
                          G_GNUC_UNUSED GIOCondition condition,
                          gpointer                   user_data)
{
        GSSDPClient *client = GSSDP_CLIENT (user_data);
        GSSDPSocketSource *request_socket = NULL;
        GError *error = NULL;
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        if (socket_source_cb (priv->request_socket, client))
                return TRUE;

        request_socket = gssdp_socket_source_new (
                                        GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                                        priv->device.host_addr,
                                        priv->socket_ttl,
                                        gssdp_client_get_interface (client),
                                        priv->device.index,
                                        &error);
        if (request_socket != NULL) {
                g_clear_object (&priv->request_socket);
                priv->request_socket = request_socket;
                gssdp_socket_source_set_callback (
                                        priv->request_socket,
                                        (GSourceFunc) request_socket_source_cb,
                                        client);
                gssdp_socket_source_attach (priv->request_socket);
        } else {
                g_warning ("Could not recreate request socket on error: %s",
                           error->message);
                g_clear_error (&error);
        }

        return TRUE;
}

static gboolean
multicast_socket_source_cb (G_GNUC_UNUSED GIOChannel  *source,
                            G_GNUC_UNUSED GIOCondition condition,
                            gpointer                   user_data)
{
        GSSDPClient *client = GSSDP_CLIENT (user_data);
        GSSDPSocketSource *multicast_socket = NULL;
        GError *error = NULL;
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        if (socket_source_cb (priv->multicast_socket, client))
                return TRUE;

        multicast_socket = gssdp_socket_source_new (
                                        GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                                        priv->device.host_addr,
                                        priv->socket_ttl,
                                        gssdp_client_get_interface (client),
                                        priv->device.index,
                                        &error);
        if (multicast_socket != NULL) {
                g_clear_object (&priv->multicast_socket);
                priv->multicast_socket = multicast_socket;
                gssdp_socket_source_set_callback (
                                        priv->multicast_socket,
                                        (GSourceFunc)multicast_socket_source_cb,
                                        client);
                gssdp_socket_source_attach (priv->multicast_socket);
        } else {
                g_warning ("Could not recreate search socket on error: %s",
                           error->message);
                g_clear_error (&error);
        }

        return TRUE;
}

static gboolean
search_socket_source_cb (G_GNUC_UNUSED GIOChannel  *source,
                         G_GNUC_UNUSED GIOCondition condition,
                         gpointer                   user_data)
{
        GSSDPClient *client = GSSDP_CLIENT (user_data);
        GSSDPSocketSource *search_socket = NULL;
        GError *error = NULL;
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        if (socket_source_cb (priv->search_socket, client))
                return TRUE;

        search_socket = gssdp_socket_source_new (
                                        GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                                        priv->device.host_addr,
                                        priv->socket_ttl,
                                        gssdp_client_get_interface (client),
                                        priv->device.index,
                                        &error);
        if (search_socket != NULL) {
                g_clear_object (&priv->search_socket);
                priv->search_socket = search_socket;
                gssdp_socket_source_set_callback (
                                        priv->search_socket,
                                        (GSourceFunc)search_socket_source_cb,
                                        client);
                gssdp_socket_source_attach (priv->search_socket);
        } else {
                g_warning ("Could not recreate search socket on error: %s",
                           error->message);
                g_clear_error (&error);
        }

        return TRUE;
}

static gboolean
init_network_info (GSSDPClient *client, GError **error)
{
        GSSDPClientPrivate *priv = gssdp_client_get_instance_private (client);

        /* If we were constructed with a host_ip, try to parse a host_addr from that.
         * Further code will only work with host_addr */
        if (priv->device.host_ip != NULL) {
                GInetAddress *addr =
                        g_inet_address_new_from_string (priv->device.host_ip);
                if (addr == NULL) {
                        g_set_error (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_FAILED,
                                     "Unparseable host_ip %s",
                                     priv->device.host_ip);

                        return FALSE;
                }

                // If there was also a host address passed (why?!) make sure
                // they match up, otherwise exit with error as well
                if (priv->device.host_addr != NULL) {
                        gboolean equal =
                                g_inet_address_equal (priv->device.host_addr,
                                                      addr);
                        g_object_unref (addr);
                        if (!equal) {
                                g_set_error_literal (
                                        error,
                                        GSSDP_ERROR,
                                        GSSDP_ERROR_FAILED,
                                        "host_ip and host_addr do not match");
                                return FALSE;
                        }

                } else {
                        priv->device.host_addr = addr;
                }

        }

        /* Either interface name or host_ip wasn't given during construction.
         * If one is given, try to find the other, otherwise just pick an
         * interface.
         */
        if (priv->device.iface_name == NULL || priv->device.host_addr == NULL ||
            priv->device.host_mask == NULL) {
                if (!gssdp_net_get_host_ip (&(priv->device), error))
                        return FALSE;
        } else {
                /* Ugly. Ideally, get_host_ip needs to be run everytime, but
                 * it is currently to stupid so just query index here if we
                 * have a name and an interface already.
                 *
                 * query_ifindex will return -1 on platforms that don't
                 * support this.
                 */
                priv->device.index =
                        gssdp_net_query_ifindex (&priv->device);

                priv->device.address_family =  g_inet_address_get_family
                                        (priv->device.host_addr);
        }

        if (priv->device.iface_name == NULL) {
                g_set_error_literal (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_FAILED,
                                     "No default route?");

                return FALSE;
        } else if (priv->device.host_addr == NULL) {
                g_set_error (error,
                             GSSDP_ERROR,
                             GSSDP_ERROR_NO_IP_ADDRESS,
                             "Failed to find IP of interface %s",
                             priv->device.iface_name);

                return FALSE;
        } else if (priv->device.host_mask == NULL) {
                g_set_error (error,
                             GSSDP_ERROR,
                             GSSDP_ERROR_FAILED,
                             "No network mask?");
                return FALSE;
        }

        g_debug ("Created SSDP client %p", client);
        g_debug ("  iface_name : %s", priv->device.iface_name);
        g_debug ("  host_ip    : %s", gssdp_client_get_host_ip (client));
        g_debug ("  port       : %u", gssdp_client_get_port (client));
        g_debug ("  server_id  : %s", priv->server_id);
        g_debug ("  network    : %s", priv->device.network);
        g_debug ("  index      : %d", priv->device.index);
        g_debug ("  host_addr  : %p", priv->device.host_addr);
        g_debug ("  host_mask  : %p", priv->device.host_mask);

        return TRUE;
}
