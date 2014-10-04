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

/**
 * SECTION:gssdp-client
 * @short_description: SSDP "bus" wrapper.
 *
 * #GSSDPClient wraps the SSDP "bus" as used by both #GSSDPResourceBrowser
 * and #GSSDPResourceGroup.
 */

#include <config.h>
#include <sys/types.h>
#include <glib.h>
#ifndef G_OS_WIN32
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#define _WIN32_WINNT 0x502
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
typedef int socklen_t;
/* from the return value of inet_addr */
typedef unsigned long in_addr_t;
#endif
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#ifndef G_OS_WIN32
#include <arpa/inet.h>
#include <net/if.h>
#ifndef __BIONIC__
#include <ifaddrs.h>
#else
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdlib.h>
#endif
#endif
#include <libsoup/soup-headers.h>

#ifdef HAVE_SIOCGIFINDEX
#include <sys/ioctl.h>
#endif

#include "gssdp-client.h"
#include "gssdp-client-private.h"
#include "gssdp-error.h"
#include "gssdp-socket-source.h"
#include "gssdp-marshal.h"
#include "gssdp-protocol.h"
#ifdef HAVE_PKTINFO
#include "gssdp-pktinfo-message.h"
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#ifdef __BIONIC__
#include <android/log.h>
#endif

/* Size of the buffer used for reading from the socket */
#define BUF_SIZE 65536

/* interface index for loopback device */
#define LOOPBACK_IFINDEX 1

static void
gssdp_client_initable_iface_init (gpointer g_iface,
                                  gpointer iface_data);

G_DEFINE_TYPE_EXTENDED (GSSDPClient,
                        gssdp_client,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE
                                (G_TYPE_INITABLE,
                                 gssdp_client_initable_iface_init));

struct _GSSDPNetworkDevice {
        char *iface_name;
        char *host_ip;
        GInetAddress *host_addr;
        char *network;
        struct sockaddr_in mask;
        gint index;
};
typedef struct _GSSDPNetworkDevice GSSDPNetworkDevice;

struct _GSSDPHeaderField {
        char *name;
        char *value;
};
typedef struct _GSSDPHeaderField GSSDPHeaderField;

struct _GSSDPClientPrivate {
        char              *server_id;

        guint              socket_ttl;
        guint              msearch_port;
        GSSDPNetworkDevice device;
        GList             *headers;

        GSSDPSocketSource *request_socket;
        GSSDPSocketSource *multicast_socket;
        GSSDPSocketSource *search_socket;

        gboolean           active;
        gboolean           initialized;
};

enum {
        PROP_0,
        PROP_MAIN_CONTEXT,
        PROP_SERVER_ID,
        PROP_IFACE,
        PROP_NETWORK,
        PROP_HOST_IP,
        PROP_ACTIVE,
        PROP_SOCKET_TTL,
        PROP_MSEARCH_PORT,
};

enum {
        MESSAGE_RECEIVED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static char *
make_server_id                (void);
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
        client->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (client,
                                         GSSDP_TYPE_CLIENT,
                                         GSSDPClientPrivate);

        client->priv->active = TRUE;

        /* Generate default server ID */
        client->priv->server_id = make_server_id ();
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
        GError *internal_error = NULL;

        if (client->priv->initialized)
                return TRUE;

#ifdef G_OS_WIN32
        WSADATA wsaData = {0};
        if (WSAStartup (MAKEWORD (2,2), &wsaData) != 0) {
                gchar *message;

                message = g_win32_error_message (WSAGetLastError ());
                g_set_error_literal (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_FAILED,
                                     message);
                g_free (message);

                return FALSE;
        }
#endif

        /* Make sure all network info is available to us */
        if (!init_network_info (client, &internal_error))
                goto errors;

        /* Set up sockets (Will set errno if it failed) */
        client->priv->request_socket =
                gssdp_socket_source_new (GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                                         gssdp_client_get_host_ip (client),
                                         client->priv->socket_ttl,
                                         client->priv->device.iface_name,
                                         &internal_error);
        if (client->priv->request_socket != NULL) {
                gssdp_socket_source_set_callback
                        (client->priv->request_socket,
                        (GSourceFunc) request_socket_source_cb,
                        client);
        } else {
                goto errors;
        }

        client->priv->multicast_socket =
                gssdp_socket_source_new (GSSDP_SOCKET_SOURCE_TYPE_MULTICAST,
                                         gssdp_client_get_host_ip (client),
                                         client->priv->socket_ttl,
                                         client->priv->device.iface_name,
                                         &internal_error);
        if (client->priv->multicast_socket != NULL) {
                gssdp_socket_source_set_callback
                        (client->priv->multicast_socket,
                         (GSourceFunc) multicast_socket_source_cb,
                         client);
        } else {
                goto errors;
        }

        /* Setup send socket. For security reasons, it is not recommended to
         * send M-SEARCH with source port == SSDP_PORT */
        client->priv->search_socket = GSSDP_SOCKET_SOURCE (g_initable_new
                                        (GSSDP_TYPE_SOCKET_SOURCE,
                                         NULL,
                                         &internal_error,
                                         "type", GSSDP_SOCKET_SOURCE_TYPE_SEARCH,
                                         "host-ip", gssdp_client_get_host_ip (client),
                                         "ttl", client->priv->socket_ttl,
                                         "port", client->priv->msearch_port,
                                         "device-name", client->priv->device.iface_name,
                                         NULL));

        if (client->priv->search_socket != NULL) {
                gssdp_socket_source_set_callback
                                        (client->priv->search_socket,
                                         (GSourceFunc) search_socket_source_cb,
                                         client);
        }
 errors:
        if (!client->priv->request_socket ||
            !client->priv->multicast_socket ||
            !client->priv->search_socket) {
                g_propagate_error (error, internal_error);

                if (client->priv->request_socket) {
                        g_object_unref (client->priv->request_socket);

                        client->priv->request_socket = NULL;
                }

                if (client->priv->multicast_socket) {
                        g_object_unref (client->priv->multicast_socket);

                        client->priv->multicast_socket = NULL;
                }

                if (client->priv->search_socket) {
                        g_object_unref (client->priv->search_socket);

                        client->priv->search_socket = NULL;
                }

                return FALSE;
        }

        gssdp_socket_source_attach (client->priv->request_socket);
        gssdp_socket_source_attach (client->priv->multicast_socket);
        gssdp_socket_source_attach (client->priv->search_socket);

        client->priv->initialized = TRUE;

        return TRUE;
}

static void
gssdp_client_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
        GSSDPClient *client;

        client = GSSDP_CLIENT (object);

        switch (property_id) {
        case PROP_SERVER_ID:
                g_value_set_string
                        (value,
                         gssdp_client_get_server_id (client));
                break;
        case PROP_MAIN_CONTEXT:
                g_warning ("GSSDPClient:main-context is deprecated."
                           " Please use g_main_context_push_thread_default()");
                g_value_set_pointer
                        (value,
                         (gpointer)
                          g_main_context_get_thread_default ());
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
        case PROP_ACTIVE:
                g_value_set_boolean (value, client->priv->active);
                break;
        case PROP_SOCKET_TTL:
                g_value_set_uint (value, client->priv->socket_ttl);
                break;
        case PROP_MSEARCH_PORT:
                g_value_set_uint (value, client->priv->msearch_port);
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
        GSSDPClient *client;

        client = GSSDP_CLIENT (object);

        switch (property_id) {
        case PROP_SERVER_ID:
                gssdp_client_set_server_id (client,
                                            g_value_get_string (value));
                break;
        case PROP_MAIN_CONTEXT:
                if (g_value_get_pointer (value) != NULL)
                        g_warning ("GSSDPClient:main-context is deprecated."
                                   " Please use g_main_context_push_thread_default()");
                break;
        case PROP_IFACE:
                client->priv->device.iface_name = g_value_dup_string (value);
                break;
        case PROP_NETWORK:
                client->priv->device.network = g_value_dup_string (value);
                break;
        case PROP_ACTIVE:
                client->priv->active = g_value_get_boolean (value);
                break;
        case PROP_SOCKET_TTL:
                client->priv->socket_ttl = g_value_get_uint (value);
                break;
        case PROP_MSEARCH_PORT:
                client->priv->msearch_port = g_value_get_uint (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_client_dispose (GObject *object)
{
        GSSDPClient *client;

        client = GSSDP_CLIENT (object);

        /* Destroy the SocketSources */
        if (client->priv->request_socket) {
                g_object_unref (client->priv->request_socket);
                client->priv->request_socket = NULL;
        }

        if (client->priv->multicast_socket) {
                g_object_unref (client->priv->multicast_socket);
                client->priv->multicast_socket = NULL;
        }

        if (client->priv->search_socket) {
                g_object_unref (client->priv->search_socket);
                client->priv->search_socket = NULL;
        }

        G_OBJECT_CLASS (gssdp_client_parent_class)->dispose (object);
}

static void
gssdp_client_finalize (GObject *object)
{
        GSSDPClient *client;

        client = GSSDP_CLIENT (object);
#ifdef G_OS_WIN32
        WSACleanup ();
#endif

        g_free (client->priv->server_id);
        g_free (client->priv->device.iface_name);
        g_free (client->priv->device.host_ip);
        g_free (client->priv->device.network);

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

        g_type_class_add_private (klass, sizeof (GSSDPClientPrivate));

        /**
         * GSSDPClient:server-id:
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
         * GSSDPClient:main-context: (skip)
         *
         * The #GMainContext to use. Set to NULL to use the default.
         * Deprecated: 0.11.2: Use g_main_context_push_thread_default().
         **/
        g_object_class_install_property
                (object_class,
                 PROP_MAIN_CONTEXT,
                 g_param_spec_pointer
                         ("main-context",
                          "Main context",
                          "The associated GMainContext.",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPClient:interface:
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
                          G_PARAM_STATIC_NAME |
                          G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPClient:network:
         *
         * The network this client is currently connected to. You could set this
         * to anything you want to identify the network this client is
         * associated with. If you are using #GUPnPContextManager and associated
         * interface is a WiFi interface, this property is set to the ESSID of
         * the network. Otherwise, expect this to be the network IP address by
         * default.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_NETWORK,
                 g_param_spec_string
                         ("network",
                          "Network ID",
                          "The network this client is currently connected to.",
                          NULL,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_NAME |
                          G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPClient:host-ip:
         *
         * The IP address of the assoicated network interface.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_HOST_IP,
                 g_param_spec_string ("host-ip",
                                      "Host IP",
                                      "The IP address of the associated"
                                      "network interface",
                                      NULL,
                                      G_PARAM_READABLE |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GSSDPClient:active:
         *
         * Whether this client is active or not (passive). When active
         * (default), the client sends messages on the network, otherwise
         * not. In most cases, you don't want to touch this property.
         *
         **/
        g_object_class_install_property
                (object_class,
                 PROP_ACTIVE,
                 g_param_spec_boolean
                         ("active",
                          "Active",
                          "TRUE if the client is active.",
                          TRUE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME |
                          G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPClient:socket-ttl:
         *
         * Time-to-live value to use for all sockets created by this client.
         * If not set (or set to 0) the value recommended by UPnP will be used.
         * This property can only be set during object construction.
         */
        g_object_class_install_property
                (object_class,
                 PROP_SOCKET_TTL,
                 g_param_spec_uint
                        ("socket-ttl",
                         "Socket TTL",
                         "Time To Live for client's sockets",
                         0, 255,
                         0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                         G_PARAM_STATIC_BLURB));

        /**
         * GSSDPClient:msearch-port:
         *
         * UDP port to use for sending multicast M-SEARCH requests on the
         * network. If not set (or set to 0) a random port will be used.
         * This property can be only set during object construction.
         */
        g_object_class_install_property
                (object_class,
                 PROP_MSEARCH_PORT,
                 g_param_spec_uint
                        ("msearch-port",
                         "M-SEARCH port",
                         "UDP port to use for M-SEARCH requests",
                         0, G_MAXUINT16,
                         0,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
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
                              NULL, NULL,
                              gssdp_marshal_VOID__STRING_UINT_INT_BOXED,
                              G_TYPE_NONE,
                              4,
                              G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                              G_TYPE_UINT,
                              G_TYPE_INT,
                              G_TYPE_POINTER);
}

/**
 * gssdp_client_new:
 * @main_context: (allow-none): Deprecated: 0.11.2: Always set to NULL. If you want to
 *                specify a context use g_main_context_push_thread_default()
 * @iface: (allow-none): The name of the network interface, or %NULL for auto-detection.
 * @error: Location to store error, or NULL
 *
 * Return value: A new #GSSDPClient object.
 **/
GSSDPClient *
gssdp_client_new (GMainContext *main_context,
                  const char   *iface,
                  GError      **error)
{
        if (main_context) {
                g_warning ("GSSDPClient:main-context is deprecated."
                           " Please use g_main_context_push_thread_default()");
        }

        return g_initable_new (GSSDP_TYPE_CLIENT,
                               NULL,
                               error,
                               "interface", iface,
                               NULL);
}

/**
 * gssdp_client_new_with_port:
 * @iface: (allow-none): The name of the network interface, or %NULL for
 * auto-detection.
 * @msearch_port: The network port to use for M-SEARCH requests or 0 for
 * random.
 * @error: (allow-none): Location to store error, or %NULL.
 *
 * Return value: A new #GSSDPClient object.
 **/
GSSDPClient *
gssdp_client_new_with_port (const char *iface,
                            guint16     msearch_port,
                            GError    **error)
{
        return g_initable_new (GSSDP_TYPE_CLIENT,
                               NULL,
                               error,
                               "interface", iface,
                               "msearch-port", msearch_port,
                               NULL);
}

/*
 * gssdp_client_get_main_context: (skip)
 * @client: A #GSSDPClient
 *
 * Returns: (transfer none): The #GMainContext @client is associated with, or %NULL.
 * Deprecated: 0.11.2: Returns g_main_context_get_thread_default()
 **/
GMainContext *
gssdp_client_get_main_context (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);

        return g_main_context_get_thread_default ();
}

/**
 * gssdp_client_set_server_id:
 * @client: A #GSSDPClient
 * @server_id: The server ID
 *
 * Sets the server ID of @client to @server_id.
 **/
void
gssdp_client_set_server_id (GSSDPClient *client,
                            const char  *server_id)
{
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        if (client->priv->server_id) {
                g_free (client->priv->server_id);
                client->priv->server_id = NULL;
        }

        if (server_id)
                client->priv->server_id = g_strdup (server_id);

        g_object_notify (G_OBJECT (client), "server-id");
}

/**
 * gssdp_client_get_server_id:
 * @client: A #GSSDPClient
 *
 * Return value: The server ID.
 **/
const char *
gssdp_client_get_server_id (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);

        return client->priv->server_id;
}

/**
 * gssdp_client_get_interface:
 * @client: A #GSSDPClient
 *
 * Get the name of the network interface associated to @client.
 *
 * Return value: The network interface name. This string should not be freed.
 **/
const char *
gssdp_client_get_interface (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);

        return client->priv->device.iface_name;
}

/**
 * gssdp_client_get_host_ip:
 * @client: A #GSSDPClient
 *
 * Get the IP address we advertise ourselves as using.
 *
 * Return value: The IP address. This string should not be freed.
 **/
const char *
gssdp_client_get_host_ip (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);

        return client->priv->device.host_ip;
}

/**
 * gssdp_client_set_network:
 * @client: A #GSSDPClient
 * @network: The string identifying the network
 *
 * Sets the network identification of @client to @network.
 **/
void
gssdp_client_set_network (GSSDPClient *client,
                          const char  *network)
{
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        if (client->priv->device.network) {
                g_free (client->priv->device.network);
                client->priv->device.network = NULL;
        }

        if (network)
                client->priv->device.network = g_strdup (network);

        g_object_notify (G_OBJECT (client), "network");
}

/**
 * gssdp_client_get_network:
 * @client: A #GSSDPClient
 *
 * Get the network this client is associated with.
 *
 * Return value: The network identification. This string should not be freed.
 **/
const char *
gssdp_client_get_network (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);

        return client->priv->device.network;
}

/**
 * gssdp_client_get_active:
 * @client: A #GSSDPClient
 *
 * Return value: %TRUE if @client is active, %FALSE otherwise.
 **/
gboolean
gssdp_client_get_active (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), FALSE);

        return client->priv->active;
}

static void
header_field_free (GSSDPHeaderField *header)
{
        g_free (header->name);
        g_free (header->value);
        g_slice_free (GSSDPHeaderField, header);
}

static gchar *
append_header_fields (GSSDPClient *client,
                      const gchar *message)
{
        GString *str;
        GList *iter;

        str = g_string_new (message);

        for (iter = client->priv->headers; iter; iter = iter->next) {
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
 * @value: Header value
 *
 * Adds a header field to the message sent by this @client. It is intended to
 * be used by clients requiring vendor specific header fields. (If there is an
 * existing header with name name , then this creates a second one).
 **/
void
gssdp_client_append_header (GSSDPClient *client,
                            const char  *name,
                            const char  *value)
{
        GSSDPHeaderField *header;

        g_return_if_fail (GSSDP_IS_CLIENT (client));
        g_return_if_fail (name != NULL);

        header = g_slice_new (GSSDPHeaderField);
        header->name = g_strdup (name);
        header->value = g_strdup (value);
        client->priv->headers = g_list_append (client->priv->headers, header);
}

/**
 * gssdp_client_remove_header:
 * @client: A #GSSDPClient
 * @name: Header name
 *
 * Removes @name from the list of headers . If there are multiple values for
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

        priv = client->priv;
        l = priv->headers;
        while (l != NULL)
        {
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
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        g_list_free_full (client->priv->headers,
                          (GDestroyNotify) header_field_free);
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
        gssize res;
        GError *error = NULL;
        GInetAddress *inet_address = NULL;
        GSocketAddress *address = NULL;
        GSocket *socket;
        char *extended_message;

        g_return_if_fail (GSSDP_IS_CLIENT (client));
        g_return_if_fail (message != NULL);

        if (!client->priv->active)
                /* We don't send messages in passive mode */
                return;

        /* Broadcast if @dest_ip is NULL */
        if (dest_ip == NULL)
                dest_ip = SSDP_ADDR;

        /* Use default port if no port was explicitly specified */
        if (dest_port == 0)
                dest_port = SSDP_PORT;

        if (type == _GSSDP_DISCOVERY_REQUEST)
                socket = gssdp_socket_source_get_socket
                                        (client->priv->search_socket);
        else
                socket = gssdp_socket_source_get_socket
                                        (client->priv->request_socket);

        inet_address = g_inet_address_new_from_string (dest_ip);
        address = g_inet_socket_address_new (inet_address, dest_port);
        extended_message = append_header_fields (client, message);

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

/*
 * Generates the default server ID
 */
static char *
make_server_id (void)
{
#ifdef G_OS_WIN32
        OSVERSIONINFO versioninfo;
        versioninfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
        if (GetVersionEx (&versioninfo)) {
                return g_strdup_printf ("Microsoft Windows/%ld.%ld GSSDP/%s",
                                        versioninfo.dwMajorVersion,
                                        versioninfo.dwMinorVersion,
                                        VERSION);
        } else {
                return g_strdup_printf ("Microsoft Windows GSSDP/%s",
                                        VERSION);
        }
#else
        struct utsname sysinfo;

        uname (&sysinfo);
        
        return g_strdup_printf ("%s/%s GSSDP/%s",
                                sysinfo.sysname,
                                sysinfo.version,
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

                if (path)
                        g_free (path);

                return TRUE;
        } else {
                soup_message_headers_free (*headers);
                *headers = NULL;

                if (path)
                        g_free (path);

                if (req_method)
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
                soup_message_headers_free (*headers);
                *headers = NULL;

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
        GSocketControlMessage **messages;
        gint num_messages;

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
                g_warning ("Failed to receive from socket: %s",
                           error->message);

                goto out;
        }

#ifdef HAVE_PKTINFO
        {
                int i;
                for (i = 0; i < num_messages; i++) {
                        GSSDPPktinfoMessage *msg;
                        gint msg_ifindex;

                        if (!GSSDP_IS_PKTINFO_MESSAGE (messages[i]))
                                continue;

                        msg = GSSDP_PKTINFO_MESSAGE (messages[i]);
                        msg_ifindex = gssdp_pktinfo_message_get_ifindex (msg);
                        /* message needs to be on correct interface or on
                         * loopback (as kernel can be smart and route things
                         * there even if sent to another network) */
                        if (!((msg_ifindex == client->priv->device.index ||
                               msg_ifindex == LOOPBACK_IFINDEX) &&
                              (g_inet_address_equal (gssdp_pktinfo_message_get_local_addr (msg),
                                                     client->priv->device.host_addr))))
                                goto out;
                        else
                                break;
                }
        }
#else
        /* We need the following lines to make sure the right client received
         * the packet. We won't need to do this if there was any way to tell
         * Mr. Unix that we are only interested in receiving multicast packets
         * on this socket from a particular interface but AFAIK that is not
         * possible, at least not in a portable way.
         */
        {
                struct sockaddr_in addr;
                in_addr_t mask;
                in_addr_t our_addr;
                if (!g_socket_address_to_native (address,
                                                 &addr,
                                                 sizeof (struct sockaddr_in),
                                                 &error)) {
                        g_warning ("Could not convert address to native: %s",
                                   error->message);

                        goto out;
                }

                mask = client->priv->device.mask.sin_addr.s_addr;
                our_addr = inet_addr (gssdp_client_get_host_ip (client));

                if ((addr.sin_addr.s_addr & mask) != (our_addr & mask))
                        goto out;

        }
#endif

        if (bytes >= BUF_SIZE) {
                g_warning ("Received packet of %u bytes, but the maximum "
                           "buffer size is %d. Packed dropped.",
                           (unsigned int) bytes, BUF_SIZE);

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
                g_signal_emit (client,
                               signals[MESSAGE_RECEIVED],
                               0,
                               ip_string,
                               port,
                               type,
                               headers);
        }

out:
        if (error)
                g_error_free (error);

        if (ip_string)
                g_free (ip_string);

        if (headers)
                soup_message_headers_free (headers);

        if (address)
                g_object_unref (address);

        if (messages) {
                int i;
                for (i = 0; i < num_messages; i++)
                        g_object_unref (messages[i]);

                g_free (messages);
        }

        return TRUE;
}

static gboolean
request_socket_source_cb (G_GNUC_UNUSED GIOChannel  *source,
                          G_GNUC_UNUSED GIOCondition condition,
                          gpointer                   user_data)
{
        GSSDPClient *client;

        client = GSSDP_CLIENT (user_data);

        return socket_source_cb (client->priv->request_socket, client);
}

static gboolean
multicast_socket_source_cb (G_GNUC_UNUSED GIOChannel  *source,
                            G_GNUC_UNUSED GIOCondition condition,
                            gpointer                   user_data)
{
        GSSDPClient *client;

        client = GSSDP_CLIENT (user_data);

        return socket_source_cb (client->priv->multicast_socket, client);
}

static gboolean
search_socket_source_cb (G_GNUC_UNUSED GIOChannel  *source,
                         G_GNUC_UNUSED GIOCondition condition,
                         gpointer                   user_data)
{
        GSSDPClient *client;

        client = GSSDP_CLIENT (user_data);

        return socket_source_cb (client->priv->search_socket, client);
}

#ifdef G_OS_WIN32
static gboolean
is_primary_adapter (PIP_ADAPTER_ADDRESSES adapter)
{
        int family =
                adapter->FirstUnicastAddress->Address.lpSockaddr->sa_family;

        return !(adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
                 family == AF_INET6);
}

static gboolean
extract_address_and_prefix (PIP_ADAPTER_UNICAST_ADDRESS  adapter,
                            PIP_ADAPTER_PREFIX           prefix,
                            char                        *iface,
                            char                        *network) {
        DWORD ret = 0;
        DWORD len = INET6_ADDRSTRLEN;

        ret = WSAAddressToStringA (adapter->Address.lpSockaddr,
                                   adapter->Address.iSockaddrLength,
                                   NULL,
                                   iface,
                                   &len);
        if (ret != 0)
                return FALSE;

        if (prefix) {
                ret = WSAAddressToStringA (prefix->Address.lpSockaddr,
                                           prefix->Address.iSockaddrLength,
                                           NULL,
                                           network,
                                           &len);
                if (ret != 0)
                        return FALSE;
        } else if (strcmp (iface, "127.0.0.1"))
                strcpy (network, "127.0.0.0");
        else
                return FALSE;

        return TRUE;
}
#endif

static int
query_ifindex (const char *iface_name)
{
#ifdef HAVE_SIOCGIFINDEX
        int fd;
        int result;
        struct ifreq ifr;

        fd = socket (AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
                return -1;

        memset (&ifr, 0, sizeof(struct ifreq));
        strncpy (ifr.ifr_ifrn.ifrn_name, iface_name, IFNAMSIZ);

        result = ioctl (fd, SIOCGIFINDEX, (char *)&ifr);
        close (fd);

        if (result == 0)
                return ifr.ifr_ifindex;
        else
                return -1;
#else
        return -1;
#endif
}

/*
 * Get the host IP for the specified interface. If no interface is specified,
 * it gets the IP of the first up & running interface and sets @interface
 * appropriately.
 */

static gboolean
get_host_ip (GSSDPNetworkDevice *device)
{
#ifdef G_OS_WIN32
        GList *up_ifaces = NULL, *ifaceptr = NULL;
        ULONG flags = GAA_FLAG_INCLUDE_PREFIX |
                      GAA_FLAG_SKIP_DNS_SERVER |
                      GAA_FLAG_SKIP_MULTICAST;
        DWORD size = 15360; /* Use 15k buffer initially as documented in MSDN */
        DWORD ret;
        PIP_ADAPTER_ADDRESSES adapters_addresses;
        PIP_ADAPTER_ADDRESSES adapter;

        do {
                adapters_addresses = (PIP_ADAPTER_ADDRESSES) g_malloc0 (size);
                ret = GetAdaptersAddresses (AF_UNSPEC,
                                            flags,
                                            NULL,
                                            adapters_addresses,
                                            &size);
                if (ret == ERROR_BUFFER_OVERFLOW)
                        g_free (adapters_addresses);
        } while (ret == ERROR_BUFFER_OVERFLOW);

        if (ret == ERROR_SUCCESS)
                for (adapter = adapters_addresses;
                     adapter != NULL;
                     adapter = adapter->Next) {
                        if (adapter->FirstUnicastAddress == NULL)
                                continue;
                        if (adapter->OperStatus != IfOperStatusUp)
                                continue;
                        /* skip Point-to-Point devices */
                        if (adapter->IfType == IF_TYPE_PPP)
                                continue;

                        if (device->iface_name != NULL &&
                            strcmp (device->iface_name, adapter->AdapterName) != 0)
                                continue;

                        /* I think that IPv6 is done via pseudo-adapters, so
                         * that there are either IPv4 or IPv6 addresses defined
                         * on the adapter.
                         * Loopback-Devices and IPv6 go to the end of the list,
                         * IPv4 to the front
                         */
                        if (is_primary_adapter (adapter))
                                up_ifaces = g_list_prepend (up_ifaces, adapter);
                        else
                                up_ifaces = g_list_append (up_ifaces, adapter);
                }

        for (ifaceptr = up_ifaces;
             ifaceptr != NULL;
             ifaceptr = ifaceptr->next) {
                char ip[INET6_ADDRSTRLEN];
                char prefix[INET6_ADDRSTRLEN];
                const char *p, *q;
                PIP_ADAPTER_ADDRESSES adapter;
                PIP_ADAPTER_UNICAST_ADDRESS address;

                p = NULL;

                adapter = (PIP_ADAPTER_ADDRESSES) ifaceptr->data;
                address = adapter->FirstUnicastAddress;

                if (address->Address.lpSockaddr->sa_family != AF_INET)
                        continue;

                if (extract_address_and_prefix (address,
                                                adapter->FirstPrefix,
                                                ip,
                                                prefix)) {
                                                p = ip;
                                                q = prefix;
                }

                if (p != NULL) {
                        device->host_ip = g_strdup (p);
                        /* This relies on the compiler doing an arithmetic
                         * shift here!
                         */
                        gint32 mask = 0;
                        if (adapter->FirstPrefix->PrefixLength > 0) {
                                mask = (gint32) 0x80000000;
                                mask >>= adapter->FirstPrefix->PrefixLength - 1;
                        }
                        device->mask.sin_family = AF_INET;
                        device->mask.sin_port = 0;
                        device->mask.sin_addr.s_addr = htonl ((guint32) mask);

                        if (device->iface_name == NULL)
                                device->iface_name = g_strdup (adapter->AdapterName);
                        if (device->network == NULL)
                                device->network = g_strdup (q);
                        break;
                }

        }
        g_list_free (up_ifaces);
        g_free (adapters_addresses);

        return TRUE;
#elif __BIONIC__
        struct      ifreq *ifaces = NULL;
        struct      ifreq *iface = NULL;
        struct      ifreq tmp_iface;
        struct      ifconf ifconfigs;
        struct      sockaddr_in *address, *netmask;
        struct      in_addr net_address;
        uint32_t    ip;
        int         if_buf_size, sock, i, if_num;
        GList       *if_ptr, *if_list = NULL;

        if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
                __android_log_write (ANDROID_LOG_WARN,
                                     "gssdp",
                                     "Couldn't create socket");
                return FALSE;
        }

        /* Fill ifaces with the available interfaces
         * we incrementally proceed in chunks of 4
         * till getting the full list
         */

        if_buf_size = 0;
        do {
                if_buf_size += 4 * sizeof (struct ifreq);
                ifaces = g_realloc (ifaces, if_buf_size);
                ifconfigs.ifc_len = if_buf_size;
                ifconfigs.ifc_buf = (char *) ifaces;

                /* FIXME: IPv4 only. This ioctl only deals with AF_INET */
                if (ioctl (sock, SIOCGIFCONF, &ifconfigs) == -1) {
                        __android_log_print (ANDROID_LOG_WARN, "gssdp",
                                "Couldn't get list of devices. Asked for: %d",
                                if_buf_size / sizeof (struct ifreq));

                        goto fail;
                }

        } while (ifconfigs.ifc_len >= if_buf_size);

        if_num = ifconfigs.ifc_len / sizeof (struct ifreq);

        if (!device->iface_name) {
                __android_log_print (ANDROID_LOG_DEBUG, "gssdp",
                        "Got list of %d interfaces. Looking for a suitable one",
                        if_num);
        } else {
                __android_log_print (ANDROID_LOG_DEBUG, "gssdp",
                        "List of %d interfaces ready. Now finding %s",
                        if_num, device->iface_name);
        }

        /* Buildup prioritized interface list
         */

        for (i = 0; i < if_num; i++) {

                address = (struct sockaddr_in *) &(ifaces[i].ifr_addr);

                __android_log_print (ANDROID_LOG_DEBUG,
                                     "gssdp",
                                     "Trying interface: %s",
                                     ifaces[i].ifr_name);

                if (!address->sin_addr.s_addr) {
                        __android_log_write (ANDROID_LOG_DEBUG, "gssdp",
                                "No configured address. Discarding");
                        continue;
                }

                memcpy (&tmp_iface, &ifaces[i], sizeof (struct ifreq));

                if (ioctl (sock, SIOCGIFFLAGS, &tmp_iface) == -1) {
                        __android_log_write (ANDROID_LOG_DEBUG, "gssdp",
                                "Couldn't get flags. Discarding");
                        continue;
                }

                /* If an specific interface query was passed over.. */
                if (device->iface_name &&
                    g_strcmp0 (device->iface_name, tmp_iface.ifr_name)) {
                        continue;
                } else if (!(tmp_iface.ifr_flags & IFF_UP) ||
                           tmp_iface.ifr_flags & IFF_POINTOPOINT) {
                        continue;
                }

                /* Prefer non loopback */
                if (ifaces[i].ifr_flags & IFF_LOOPBACK)
                        if_list = g_list_append (if_list, ifaces + i);
                else
                        if_list = g_list_prepend (if_list, ifaces + i);

                if (device->iface_name)
                    break;
        }

        if (!g_list_length (if_list)) {
                __android_log_write (ANDROID_LOG_DEBUG,
                                     "gssdp",
                                     "No usable interfaces found");
                goto fail;
        }

        /* Fill device with data from the first interface
         * we can get complete config info for and return
         */

        for (if_ptr = if_list; if_ptr != NULL;
             if_ptr = g_list_next (if_ptr)) {

                iface   = (struct ifreq *) if_ptr->data;
                address = (struct sockaddr_in *) &(iface->ifr_addr);
                netmask = (struct sockaddr_in *) &(iface->ifr_netmask);

                device->host_ip = g_malloc0 (INET_ADDRSTRLEN);

                if (inet_ntop (AF_INET, &(address->sin_addr),
                        device->host_ip, INET_ADDRSTRLEN) == NULL) {

                        __android_log_print (ANDROID_LOG_INFO,
                                             "gssdp",
                                             "Failed to get ip for: %s, %s",
                                             iface->ifr_name,
                                             strerror (errno));

                        g_free (device->host_ip);
                        device->host_ip = NULL;
                        continue;
                }

                ip = address->sin_addr.s_addr;

                if (ioctl (sock, SIOCGIFNETMASK, iface) == -1) {
                        __android_log_write (ANDROID_LOG_DEBUG, "gssdp",
                                "Couldn't get netmask. Discarding");
                        g_free (device->host_ip);
                        device->host_ip = NULL;
                        continue;
                }

                memcpy (&device->mask, netmask, sizeof (struct sockaddr_in));

                if (device->network == NULL) {
                        device->network = g_malloc0 (INET_ADDRSTRLEN);

                        net_address.s_addr = ip & netmask->sin_addr.s_addr;

                        if (inet_ntop (AF_INET, &net_address,
                            device->network, INET_ADDRSTRLEN) == NULL) {

                                __android_log_print (ANDROID_LOG_WARN, "gssdp",
                                        "Failed to get nw for: %s, %s",
                                        iface->ifr_name, strerror (errno));

                                g_free (device->host_ip);
                                device->host_ip = NULL;
                                g_free (device->network);
                                device->network = NULL;
                                continue;
                        }
                }

                if (!device->iface_name)
                    device->iface_name = g_strdup (iface->ifr_name);

                goto success;

        }

        __android_log_write (ANDROID_LOG_WARN, "gssdp",
                "Traversed whole list without finding a configured device");

fail:
        __android_log_write (ANDROID_LOG_WARN,
                             "gssdp",
                             "Failed to get configuration for device");
        g_free (ifaces);
        g_list_free (if_list);
        close (sock);
        return FALSE;
success:
        __android_log_print (ANDROID_LOG_DEBUG, "gssdp",
                "Returned config params for device: %s ip: %s network: %s",
                device->iface_name, device->host_ip, device->network);
        g_free (ifaces);
        g_list_free (if_list);
        close (sock);
        return TRUE;
#else
        struct ifaddrs *ifa_list, *ifa;
        GList *up_ifaces, *ifaceptr;

        up_ifaces = NULL;

        if (getifaddrs (&ifa_list) != 0) {
                g_warning ("Failed to retrieve list of network interfaces: %s",
                           strerror (errno));

                return FALSE;
        }

        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL)
                        continue;

                if (device->iface_name &&
                    strcmp (device->iface_name, ifa->ifa_name) != 0)
                        continue;
                else if (!(ifa->ifa_flags & IFF_UP))
                        continue;
                else if ((ifa->ifa_flags & IFF_POINTOPOINT))
                        continue;

                /* Loopback and IPv6 interfaces go at the bottom on the list */
                if ((ifa->ifa_flags & IFF_LOOPBACK) ||
                    ifa->ifa_addr->sa_family == AF_INET6)
                        up_ifaces = g_list_append (up_ifaces, ifa);
                else
                        up_ifaces = g_list_prepend (up_ifaces, ifa);
        }

        for (ifaceptr = up_ifaces;
             ifaceptr != NULL;
             ifaceptr = ifaceptr->next) {
                char ip[INET6_ADDRSTRLEN];
                char net[INET6_ADDRSTRLEN];
                const char *p, *q;
                struct sockaddr_in *s4, *s4_mask;
                struct in_addr net_addr;
                const guint8 *bytes;

                ifa = ifaceptr->data;

                if (ifa->ifa_addr->sa_family != AF_INET) {
                        continue;
                }

                s4 = (struct sockaddr_in *) ifa->ifa_addr;
                p = inet_ntop (AF_INET,
                               &s4->sin_addr,
                               ip,
                               sizeof (ip));
                device->host_ip = g_strdup (p);

                bytes = (const guint8 *) &s4->sin_addr;
                device->host_addr = g_inet_address_new_from_bytes
                                        (bytes, G_SOCKET_FAMILY_IPV4);

                s4_mask = (struct sockaddr_in *) ifa->ifa_netmask;
                memcpy (&(device->mask), s4_mask, sizeof (struct sockaddr_in));
                net_addr.s_addr = (in_addr_t) s4->sin_addr.s_addr &
                                  (in_addr_t) s4_mask->sin_addr.s_addr;
                q = inet_ntop (AF_INET, &net_addr, net, sizeof (net));

                device->index = query_ifindex (ifa->ifa_name);

                if (device->iface_name == NULL)
                        device->iface_name = g_strdup (ifa->ifa_name);
                if (device->network == NULL)
                        device->network = g_strdup (q);
                break;
        }

        g_list_free (up_ifaces);
        freeifaddrs (ifa_list);

        return TRUE;
#endif
}

static gboolean
init_network_info (GSSDPClient *client, GError **error)
{
        gboolean ret = TRUE;

        /* Either interface name or host_ip wasn't given during construction.
         * If one is given, try to find the other, otherwise just pick an
         * interface.
         */
        if (client->priv->device.iface_name == NULL ||
            client->priv->device.host_ip == NULL)
                get_host_ip (&(client->priv->device));

        if (client->priv->device.iface_name == NULL) {
                g_set_error_literal (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_FAILED,
                                     "No default route?");

                ret = FALSE;
        } else if (client->priv->device.host_ip == NULL) {
                        g_set_error (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_NO_IP_ADDRESS,
                                     "Failed to find IP of interface %s",
                                     client->priv->device.iface_name);

                ret = FALSE;
        }

        return ret;
}

