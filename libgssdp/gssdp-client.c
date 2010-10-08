/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
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

/**
 * SECTION:gssdp-client
 * @short_description: SSDP "bus" wrapper.
 *
 * #GSSDPClient wraps the SSDP "bus" as used by both #GSSDPResourceBrowser
 * and #GSSDPResourceGroup.
 */

#include <config.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <libsoup/soup-headers.h>

#include "gssdp-client.h"
#include "gssdp-client-private.h"
#include "gssdp-error.h"
#include "gssdp-socket-source.h"
#include "gssdp-marshal.h"
#include "gssdp-protocol.h"

/* Size of the buffer used for reading from the socket */
#define BUF_SIZE 1024

G_DEFINE_TYPE (GSSDPClient,
               gssdp_client,
               G_TYPE_OBJECT);

struct _GSSDPClientPrivate {
        GMainContext      *main_context;

        char              *server_id;
        char              *interface;
        char              *host_ip;

        GError            **error;

        GSSDPSocketSource *request_socket;
        GSSDPSocketSource *multicast_socket;

        gboolean           active;
};

enum {
        PROP_0,
        PROP_MAIN_CONTEXT,
        PROP_SERVER_ID,
        PROP_IFACE,
        PROP_HOST_IP,
        PROP_ACTIVE,
        PROP_ERROR
};

enum {
        MESSAGE_RECEIVED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Function prototypes */
static void
gssdp_client_set_main_context (GSSDPClient  *client,
                               GMainContext *context);
static char *
make_server_id                (void);
static gboolean
request_socket_source_cb      (gpointer      user_data);
static gboolean
multicast_socket_source_cb    (gpointer      user_data);
static gboolean
init_network_info             (GSSDPClient  *client);

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
gssdp_client_constructed (GObject *object)
{
        GSSDPClient *client = GSSDP_CLIENT (object);

        /* Make sure all network info is available to us */
        if (!init_network_info (client))
                return;

        /* Set up sockets (Will set errno if it failed) */
        client->priv->request_socket =
                gssdp_socket_source_new (GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
                                         gssdp_client_get_host_ip (client));
        if (client->priv->request_socket != NULL) {
                g_source_set_callback
                        ((GSource *) client->priv->request_socket,
                         request_socket_source_cb,
                         client,
                         NULL);
        } else {
                goto errors;
        }

        client->priv->multicast_socket =
                gssdp_socket_source_new (GSSDP_SOCKET_SOURCE_TYPE_MULTICAST,
                                         gssdp_client_get_host_ip (client));
        if (client->priv->multicast_socket != NULL) {
                g_source_set_callback
                        ((GSource *) client->priv->multicast_socket,
                         multicast_socket_source_cb,
                         client,
                         NULL);
        }

 errors:
        if (!client->priv->request_socket || !client->priv->multicast_socket) {
                if (client->priv->error)
                        g_set_error_literal (client->priv->error,
                                             GSSDP_ERROR,
                                             GSSDP_ERROR_FAILED,
                                             g_strerror (errno));

                return;
        }

        g_source_attach ((GSource *) client->priv->request_socket,
                         client->priv->main_context);
        g_source_unref ((GSource *) client->priv->request_socket);

        g_source_attach ((GSource *) client->priv->multicast_socket,
                         client->priv->main_context);
        g_source_unref ((GSource *) client->priv->multicast_socket);
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
                g_value_set_pointer
                        (value,
                         (gpointer)
                          gssdp_client_get_main_context (client));
                break;
        case PROP_IFACE:
                g_value_set_string (value,
                                    gssdp_client_get_interface (client));
                break;
        case PROP_HOST_IP:
                g_value_set_string (value,
                                    gssdp_client_get_host_ip (client));
                break;
        case PROP_ACTIVE:
                g_value_set_boolean (value, client->priv->active);
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
                gssdp_client_set_main_context (client,
                                               g_value_get_pointer (value));
                break;
        case PROP_ERROR:
                client->priv->error = g_value_get_pointer (value);
                break;
        case PROP_IFACE:
                client->priv->interface = g_value_dup_string (value);
                break;
        case PROP_ACTIVE:
                client->priv->active = g_value_get_boolean (value);
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
                g_source_destroy ((GSource *) client->priv->request_socket);
                client->priv->request_socket = NULL;
        }

        if (client->priv->multicast_socket) {
                g_source_destroy ((GSource *) client->priv->multicast_socket);
                client->priv->multicast_socket = NULL;
        }

        /* Unref the context */
        if (client->priv->main_context) {
                g_main_context_unref (client->priv->main_context);
                client->priv->main_context = NULL;
        }

        G_OBJECT_CLASS (gssdp_client_parent_class)->dispose (object);
}

static void
gssdp_client_finalize (GObject *object)
{
        GSSDPClient *client;

        client = GSSDP_CLIENT (object);

        g_free (client->priv->server_id);
        g_free (client->priv->interface);
        g_free (client->priv->host_ip);

        G_OBJECT_CLASS (gssdp_client_parent_class)->finalize (object);
}

static void
gssdp_client_class_init (GSSDPClientClass *klass)
{
        GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->constructed = gssdp_client_constructed;
	object_class->set_property = gssdp_client_set_property;
	object_class->get_property = gssdp_client_get_property;
	object_class->dispose      = gssdp_client_dispose;
	object_class->finalize     = gssdp_client_finalize;

        g_type_class_add_private (klass, sizeof (GSSDPClientPrivate));

        /**
         * GSSDPClient:server-id
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
         * GSSDPClient:main-context
         *
         * The #GMainContext to use. Set to NULL to use the default.
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
         * GSSDPClient:error
         *
         * Internal property.
         *
         * Stability: Private
         **/
        g_object_class_install_property
                (object_class,
                 PROP_ERROR,
                 g_param_spec_pointer
                         ("error",
                          "Error",
                          "Location where to store the constructor GError, "
                          "if any.",
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPClient:interface
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
         * GSSDPClient:host-ip
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
         * GSSDPClient:active
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
         * GSSDPClient::message-received
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
                              gssdp_marshal_VOID__STRING_UINT_INT_POINTER,
                              G_TYPE_NONE,
                              4,
                              G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                              G_TYPE_UINT,
                              G_TYPE_INT,
                              G_TYPE_POINTER);
}

/**
 * gssdp_client_new
 * @main_context: The #GMainContext to associate with, or NULL
 * @interface: The name of the network interface, or %NULL for auto-detection.
 * @error: Location to store error, or NULL
 *
 * Return value: A new #GSSDPClient object.
 **/
GSSDPClient *
gssdp_client_new (GMainContext *main_context,
                  const char   *interface,
                  GError      **error)
{
        return g_object_new (GSSDP_TYPE_CLIENT,
                             "main-context", main_context,
                             "interface", interface,
                             "error", error,
                             NULL);
}

/**
 * Sets the GMainContext @client is associated with to @main_context
 **/
static void
gssdp_client_set_main_context (GSSDPClient  *client,
                               GMainContext *main_context)
{
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        /* A NULL main_context is fine */
        if (main_context)
                client->priv->main_context = g_main_context_ref (main_context);

        g_object_notify (G_OBJECT (client), "main-context");
}

/**
 * gssdp_client_get_main_context
 * @client: A #GSSDPClient
 *
 * Return value: The #GMainContext @client is associated with, or NULL.
 **/
GMainContext *
gssdp_client_get_main_context (GSSDPClient *client)
{
        g_return_val_if_fail (GSSDP_IS_CLIENT (client), NULL);

        return client->priv->main_context;
}

/**
 * gssdp_client_set_server_id
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
 * gssdp_client_get_server_id
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
 * gssdp_client_get_interface
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

        return client->priv->interface;
}

/**
 * gssdp_client_get_host_ip
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

        return client->priv->host_ip;
}

/**
 * gssdp_client_get_active
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

/**
 * _gssdp_client_send_message
 * @client: A #GSSDPClient
 * @dest_ip: The destination IP address, or NULL to broadcast
 * @dest_port: The destination port, or NULL for default
 * @message: The message to send
 *
 * Sends @message to @dest_ip.
 **/
void
_gssdp_client_send_message (GSSDPClient *client,
                            const char  *dest_ip,
                            gushort      dest_port,
                            const char  *message)
{
        struct sockaddr_in addr;
        int socket_fd, res;

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

        socket_fd = gssdp_socket_source_get_fd (client->priv->request_socket);

        memset (&addr, 0, sizeof (addr));

        addr.sin_family      = AF_INET;
        addr.sin_port        = htons (dest_port);
        addr.sin_addr.s_addr = inet_addr (dest_ip);

        res = sendto (socket_fd,
                      message,
                      strlen (message),
                      0,
                      (struct sockaddr *) &addr,
                      sizeof (addr));

        if (res == -1) {
                g_warning ("sendto: Error %d sending message: %s",
                           errno, strerror (errno));
        }
}

/**
 * Generates the default server ID
 **/
static char *
make_server_id (void)
{
        struct utsname sysinfo;

        uname (&sysinfo);
        
        return g_strdup_printf ("%s/%s GSSDP/%s",
                                sysinfo.sysname,
                                sysinfo.version,
                                VERSION);
}

static gboolean
parse_http_request (char                *buf,
                    int                  len,
                    SoupMessageHeaders **headers,
                    int                 *type)
{
        char *req_method;

        *headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_REQUEST);

        if (soup_headers_parse_request (buf,
                                        len,
                                        *headers,
                                        &req_method,
                                        NULL,
                                        NULL) == SOUP_STATUS_OK) {
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

                return TRUE;
        } else {
                soup_message_headers_free (*headers);
                *headers = NULL;

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
                                         NULL)) {
                if (status_code == 200)
                        *type = _GSSDP_DISCOVERY_RESPONSE;
                else
                        g_warning ("Unhandled status code '%d'", status_code);

                return TRUE;
        } else {
                soup_message_headers_free (*headers);
                *headers = NULL;

                return FALSE;
        }
}

/**
 * Called when data can be read from the socket
 **/
static gboolean
socket_source_cb (GSSDPSocketSource *socket, GSSDPClient *client)
{
        int fd, type, len;
        ssize_t bytes;
        char buf[BUF_SIZE], *end;
        struct sockaddr_in addr;
        socklen_t addr_size;
        SoupMessageHeaders *headers;
        struct in_addr our_addr;
        in_addr_t our_network;
        in_addr_t recv_network;

        /* Get FD */
        fd = gssdp_socket_source_get_fd (socket);

        /* Read data */
        addr_size = sizeof (addr);
        
        bytes = recvfrom (fd,
                          buf,
                          BUF_SIZE - 1, /* Leave space for trailing \0 */
                          MSG_TRUNC,
                          (struct sockaddr *) &addr,
                          &addr_size);
        if (bytes == -1) {
                g_warning ("Failed to read from socket: %d (%s)",
                           errno,
                           strerror (errno));

                return TRUE;
        }

        /* We need the following lines to make sure the right client received
         * the packet. We won't need to do this if there was any way to tell
         * Mr. Unix that we are only interested in receiving multicast packets
         * on this socket from a particular interface but AFAIK that is not
         * possible, at least not in a portable way.
         */
        recv_network = inet_netof (addr.sin_addr);
        our_addr.s_addr = inet_addr (gssdp_client_get_host_ip (client));
        our_network = inet_netof (our_addr);
        if (recv_network != our_network)
                return TRUE;

        if (bytes >= BUF_SIZE) {
                g_warning ("Received packet of %u bytes, but the maximum "
                           "buffer size is %d. Packed dropped.",
                           (unsigned int) bytes, BUF_SIZE);

                return TRUE;
        }

        /* Add trailing \0 */
        buf[bytes] = '\0';

        /* Find length */
        end = strstr (buf, "\r\n\r\n");
        if (!end) {
                g_warning ("Received packet lacks \"\\r\\n\\r\\n\" sequence. "
                           "Packed dropped.");

                return TRUE;
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
                        g_warning ("Unhandled message '%s'", buf);
                }
        }
        
        /* Emit signal if parsing succeeded */
        if (type >= 0) {
                g_signal_emit (client,
                               signals[MESSAGE_RECEIVED],
                               0,
                               inet_ntoa (addr.sin_addr),
                               ntohs (addr.sin_port),
                               type,
                               headers);
        }

        if (headers)
                soup_message_headers_free (headers);

        return TRUE;
}

static gboolean
request_socket_source_cb (gpointer user_data)
{
        GSSDPClient *client;

        client = GSSDP_CLIENT (user_data);

        return socket_source_cb (client->priv->request_socket, client);
}

static gboolean
multicast_socket_source_cb (gpointer user_data)
{
        GSSDPClient *client;

        client = GSSDP_CLIENT (user_data);

        return socket_source_cb (client->priv->multicast_socket, client);
}

/*
 * Get the host IP for the specified interface. If no interface is specified,
 * it gets the IP of the first up & running interface and sets @interface
 * appropriately.
 */
static char *
get_host_ip (char **interface)
{
        struct ifaddrs *ifa_list, *ifa;
        char *ret;
        GList *up_ifaces, *iface;

        ret = NULL;
        up_ifaces = NULL;

        if (getifaddrs (&ifa_list) != 0) {
                g_error ("Failed to retrieve list of network interfaces:\n%s\n",
                         strerror (errno));

                return NULL;
        }

        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL)
                        continue;

                if (*interface && strcmp (*interface, ifa->ifa_name) != 0)
                        continue;
                else if (!(ifa->ifa_flags & IFF_UP))
                        continue;
		else if ((ifa->ifa_flags & IFF_POINTOPOINT))
			continue;

                /* Loopback and IPv6 interfaces go at the bottom on the list */
                if (ifa->ifa_flags & IFF_LOOPBACK ||
                    ifa->ifa_addr->sa_family == AF_INET6)
                        up_ifaces = g_list_append (up_ifaces, ifa);
                else
                        up_ifaces = g_list_prepend (up_ifaces, ifa);
        }

        for (iface = up_ifaces; iface != NULL; iface = iface->next) {
                char ip[INET6_ADDRSTRLEN];
                const char *p;
                struct sockaddr_in *s4;
                struct sockaddr_in6 *s6;

                p = NULL;

                ifa = iface->data;

                switch (ifa->ifa_addr->sa_family) {
                case AF_INET:
                        s4 = (struct sockaddr_in *) ifa->ifa_addr;
                        p = inet_ntop (AF_INET,
                                       &s4->sin_addr, ip, sizeof (ip));
                        break;
                case AF_INET6:
                        s6 = (struct sockaddr_in6 *) ifa->ifa_addr;
                        p = inet_ntop (AF_INET6,
                                       &s6->sin6_addr, ip, sizeof (ip));
                        break;
                default:
                        continue; /* Unknown: ignore */
                }

                if (p != NULL) {
                        ret = g_strdup (p);

                        if (*interface == NULL)
                                *interface = g_strdup (ifa->ifa_name);
                        break;
                }
        }

        g_list_free (up_ifaces);
        freeifaddrs (ifa_list);

        return ret;
}

static gboolean
init_network_info (GSSDPClient *client)
{
        gboolean ret = TRUE;

        if (client->priv->interface == NULL || client->priv->host_ip == NULL)
                client->priv->host_ip =
                        get_host_ip (&client->priv->interface);

        if (client->priv->interface == NULL) {
                if (client->priv->error)
                        g_set_error (client->priv->error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_FAILED,
                                     "No default route?");

                ret = FALSE;
        } else if (client->priv->host_ip == NULL) {
                if (client->priv->error)
                        g_set_error (client->priv->error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_NO_IP_ADDRESS,
                                     "Failed to find IP of interface %s",
                                     client->priv->interface);

                ret = FALSE;
        }

        return ret;
}

