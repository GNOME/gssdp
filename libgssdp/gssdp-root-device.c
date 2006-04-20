/* 
 * (C) 2006 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
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
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "gssdp-root-device-private.h"
#include "gssdp-marshal.h"
#include "gssdp-protocol.h"
#include "gssdp-error.h"

/* An MX of 3 seconds by default */
#define DEFAULT_MX 3

/* Hack around G_DEFINE_TYPE hardcoding the type function name */
#define gssdp_root_device_get_type gssdp_root_device_type

G_DEFINE_TYPE (GSSDPRootDevice,
               gssdp_root_device,
               GSSDP_TYPE_DEVICE);

#undef gssdp_root_device_get_type

struct _GSSDPRootDevicePrivate {
        char   *location;

        char   *server_id;
        
        GList  *devices;

        gushort mx;

        int     socket_fd;

        guint   socket_source_id;
};

enum {
        PROP_0,
        PROP_LOCATION,
        PROP_SERVER_ID,
        PROP_DEVICES,
        PROP_MX
};

enum {
        DISCOVERABLE_AVAILABLE,
        DISCOVERABLE_UNAVAILABLE,
        ERROR,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Function prototypes */
static void
gssdp_root_device_set_location (GSSDPRootDevice *root_device,
                                const char      *location);
static void
emit_error                     (GSSDPRootDevice *root_device,
                                int              error_number);

/* SocketSource */
typedef struct {
        GSource source;

        GPollFD poll_fd;

        GSSDPRootDevice *root_device;
} SocketSource;

static gboolean
socket_source_prepare  (GSource    *source,
                        int        *timeout);
static gboolean
socket_source_check    (GSource    *source);
static gboolean
socket_source_dispatch (GSource    *source,
                        GSourceFunc callback,
                        gpointer    user_data);

GSourceFuncs socket_source_funcs = {
        socket_source_prepare,
        socket_source_check,
        socket_source_dispatch,
        NULL
};

static void
gssdp_root_device_init (GSSDPRootDevice *root_device)
{
        struct utsname sysinfo;

        root_device->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (root_device,
                                         GSSDP_TYPE_ROOT_DEVICE,
                                         GSSDPRootDevicePrivate);

        /* Generate default server ID */
        uname (&sysinfo);
        
        root_device->priv->server_id = g_strdup_printf ("%s/%s GSSDP/%s",
                                                        sysinfo.sysname,
                                                        sysinfo.version,
                                                        VERSION);

        /* Default MX value */
        root_device->priv->mx = DEFAULT_MX;

        /* Set up socket */
        root_device->priv->socket_fd = socket (AF_INET,
                                               SOCK_DGRAM,
                                               IPPROTO_UDP);
        if (root_device->priv->socket_fd != -1) {
                int on, res;
                GSource *source;
                SocketSource *socket_source;

                /* Enable broadcasting */
                on = 1;
                
                res = setsockopt (root_device->priv->socket_fd, 
                                  SOL_SOCKET,
                                  SO_BROADCAST,
                                  &on,
                                  sizeof (on));
                if (res == -1)
                        emit_error (root_device, errno);

                /* Create a GSource monitoring the socket */
                source = g_source_new (&socket_source_funcs,
                                       sizeof (SocketSource));

                socket_source = (SocketSource *) source;
                
                socket_source->poll_fd.fd     = root_device->priv->socket_fd;
                socket_source->poll_fd.events = G_IO_IN | G_IO_ERR;

                g_source_add_poll (source, &socket_source->poll_fd);

                socket_source->root_device = root_device;

                root_device->priv->socket_source_id = g_source_attach (source,
                                                                       NULL);
        } else 
                emit_error (root_device, errno);
}

/**
 * Default error handler
 **/
static void
gssdp_root_device_error (GSSDPRootDevice *device,
                         GError          *error)
{
        g_critical ("Error %d: %s",
                    error->code,
                    error->message);
}

static void
gssdp_root_device_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GSSDPRootDevice *root_device;

        root_device = GSSDP_ROOT_DEVICE (object);

        switch (property_id) {
        case PROP_LOCATION:
                g_value_set_string
                        (value,
                         gssdp_root_device_get_location (root_device));
                break;
        case PROP_SERVER_ID:
                g_value_set_string
                        (value,
                         gssdp_root_device_get_server_id (root_device));
                break;
        case PROP_DEVICES:
                g_value_set_pointer
                        (value,
                         (gpointer)
                          gssdp_root_device_get_devices (root_device));
                break;
        case PROP_MX:
                g_value_set_uint
                        (value,
                         gssdp_root_device_get_mx (root_device));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_root_device_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GSSDPRootDevice *root_device;

        root_device = GSSDP_ROOT_DEVICE (object);

        switch (property_id) {
        case PROP_LOCATION:
                gssdp_root_device_set_location (root_device,
                                                g_value_get_string (value));
                break;
        case PROP_SERVER_ID:
                gssdp_root_device_set_server_id (root_device,
                                                 g_value_get_string (value));
                break;
        case PROP_MX:
                gssdp_root_device_set_mx (root_device,
                                          g_value_get_uint (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_root_device_notify (GObject    *object,
                          GParamSpec *param_spec)
{
        GSSDPRootDevice *root_device;

        root_device = GSSDP_ROOT_DEVICE (object);
        
        if (strcmp (param_spec->name, "available") == 0) {
                gboolean available;
                struct sockaddr_in addr;
                struct ip_mreq mreq;
                int res, optname;

                available = gssdp_discoverable_get_available
                                (GSSDP_DISCOVERABLE (root_device));

                /* Bind or unbind to SSDP port */
                memset (&addr, 0, sizeof (addr));
                
                addr.sin_family      = AF_INET;
                addr.sin_addr.s_addr = htonl (INADDR_ANY);

                if (available) 
                        addr.sin_port = htons (SSDP_PORT);
                else 
                        addr.sin_port = htons (0); /* FIXME is this OK ? */

                res = bind (root_device->priv->socket_fd,
                            (struct sockaddr *) &addr,
                            sizeof (addr));
                if (res == -1)
                        emit_error (root_device, errno);
                
                /* Add or drop multicast membership */
                mreq.imr_multiaddr.s_addr = inet_addr (SSDP_ADDR);
                mreq.imr_interface.s_addr = htonl (INADDR_ANY);

                if (available)
                        optname = IP_ADD_MEMBERSHIP;
                else
                        optname = IP_DROP_MEMBERSHIP;

                res = setsockopt (root_device->priv->socket_fd,
                                  IPPROTO_IP,
                                  optname,
                                  &mreq,
                                  sizeof (mreq));
                if (res == -1)
                        emit_error (root_device, errno);

                /* XXX */
        }
}

static void
gssdp_root_device_dispose (GObject *object)
{
        GSSDPRootDevice *root_device;
        GObjectClass *object_class;

        root_device = GSSDP_ROOT_DEVICE (object);

        /* Get rid of the SocketSource */
        if (root_device->priv->socket_source_id) {
                g_source_remove (root_device->priv->socket_source_id);
                root_device->priv->socket_source_id = 0;
        }

        /* Close the socket, if it is open */
        if (root_device->priv->socket_fd != -1) {
                int res;
                
                res = close (root_device->priv->socket_fd);
                if (res == 0)
                        root_device->priv->socket_fd = -1;
                else
                        emit_error (root_device, errno);
        }

        object_class = G_OBJECT_CLASS (gssdp_root_device_parent_class);
        object_class->dispose (object);
}

static void
gssdp_root_device_finalize (GObject *object)
{
        GSSDPRootDevice *root_device;
        GObjectClass *object_class;

        root_device = GSSDP_ROOT_DEVICE (object);

        g_free (root_device->priv->location);
        g_free (root_device->priv->server_id);

        object_class = G_OBJECT_CLASS (gssdp_root_device_parent_class);
        object_class->finalize (object);
}

static void
gssdp_root_device_class_init (GSSDPRootDeviceClass *klass)
{
        GObjectClass *object_class;

        klass->error               = gssdp_root_device_error;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gssdp_root_device_set_property;
	object_class->get_property = gssdp_root_device_get_property;
	object_class->dispose      = gssdp_root_device_dispose;
	object_class->finalize     = gssdp_root_device_finalize;
	object_class->notify       = gssdp_root_device_notify;

        g_type_class_add_private (klass, sizeof (GSSDPRootDevicePrivate));

        g_object_class_install_property
                (object_class,
                 PROP_LOCATION,
                 g_param_spec_string
                         ("location",
                          "Location",
                          "The device information URL.",
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_SERVER_ID,
                 g_param_spec_string
                         ("server-id",
                          "Server ID",
                          "The server identifier.",
                          NULL,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_DEVICES,
                 g_param_spec_pointer
                         ("devices",
                          "Devices",
                          "The list of contained devices.",
                          G_PARAM_READABLE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_MX,
                 g_param_spec_uint
                         ("mx",
                          "MX",
                          "Maximum number of seconds in which to request "
                          "other parties to respond.",
                          1,
                          G_MAXUSHORT,
                          DEFAULT_MX,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        signals[DISCOVERABLE_AVAILABLE] =
                g_signal_new ("discoverable-available",
                              GSSDP_TYPE_ROOT_DEVICE,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GSSDPRootDeviceClass,
                                               discoverable_available),
                              NULL, NULL,
                              gssdp_marshal_VOID__STRING_STRING_STRING,
                              G_TYPE_NONE,
                              3,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING);

        signals[DISCOVERABLE_UNAVAILABLE] =
                g_signal_new ("discoverable-unavailable",
                              GSSDP_TYPE_ROOT_DEVICE,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GSSDPRootDeviceClass,
                                               discoverable_unavailable),
                              NULL, NULL,
                              gssdp_marshal_VOID__STRING_STRING,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
                              G_TYPE_STRING);

        signals[ERROR] =
                g_signal_new ("error",
                              GSSDP_TYPE_ROOT_DEVICE,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GSSDPRootDeviceClass,
                                               error),
                              NULL, NULL,
                              gssdp_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER);
}

/**
 * gssdp_device_new
 * @type: A string identifying the type of the device
 * @version: A #gushort identifying the version of the device type
 * @location: The device information URL
 *
 * Return value: A new #GSSDPRootDevice object.
 **/
GSSDPRootDevice *
gssdp_root_device_new (const char *type,
                       gushort     version,
                       const char *location)
{
        return g_object_new (GSSDP_TYPE_ROOT_DEVICE,
                             "type",     type,
                             "version",  version,
                             "location", location,
                             NULL);
}

/**
 * Sets the location URL of @root_device to @location.
 **/
static void
gssdp_root_device_set_location (GSSDPRootDevice *root_device,
                                const char      *location)
{
        root_device->priv->location = g_strdup (location);

        g_object_notify (G_OBJECT (root_device), "location");
}

/**
 * gssdp_root_device_get_location
 * @root_device: A #GSSDPRootDevice
 *
 * Return value: The device information URL.
 **/
const char *
gssdp_root_device_get_location (GSSDPRootDevice *root_device)
{
        g_return_val_if_fail (GSSDP_IS_ROOT_DEVICE (root_device), NULL);

        return root_device->priv->location;
}

/**
 * gssdp_root_device_set_server_id
 * @root_device: A #GSSDPRootDevice
 * @server_id: The server ID
 *
 * Sets the server ID of @root_device to @service_id.
 **/
void
gssdp_root_device_set_server_id (GSSDPRootDevice *root_device,
                                 const char      *server_id)
{
        g_return_if_fail (GSSDP_IS_ROOT_DEVICE (root_device));

        if (root_device->priv->server_id) {
                g_free (root_device->priv->server_id);
                root_device->priv->server_id = NULL;
        }

        if (server_id)
                root_device->priv->server_id = g_strdup (server_id);

        g_object_notify (G_OBJECT (root_device), "server-id");
}

/**
 * gssdp_root_device_get_server_id
 * @root_device: A #GSSDPRootDevice
 *
 * Return value: The server ID.
 **/
const char *
gssdp_root_device_get_server_id (GSSDPRootDevice *root_device)
{
        g_return_val_if_fail (GSSDP_IS_ROOT_DEVICE (root_device), NULL);

        return root_device->priv->server_id;
}

/**
 * gssdp_root_device_set_mx
 * @root_device: A #GSSDPRootDevice
 * @mx: The to be used MX value
 *
 * Sets the used MX value of @root_device to @mx.
 **/
void
gssdp_root_device_set_mx (GSSDPRootDevice *root_device,
                          gushort          mx)
{
        g_return_if_fail (GSSDP_IS_ROOT_DEVICE (root_device));

        if (root_device->priv->mx != mx) {
                root_device->priv->mx = mx;
                
                g_object_notify (G_OBJECT (root_device), "mx");
        }
}

/**
 * gssdp_root_device_get_mx
 * @root_device: A #GSSDPRootDevice
 *
 * Return value: The used MX value.
 **/
gushort
gssdp_root_device_get_mx (GSSDPRootDevice *root_device)
{
        g_return_val_if_fail (GSSDP_IS_ROOT_DEVICE (root_device), 0);

        return root_device->priv->mx;
}

/**
 * Adds @device to the list of devices contained in @root_device
 **/
void
gssdp_root_device_add_device (GSSDPRootDevice *root_device,
                              GSSDPDevice     *device)
{
        g_return_if_fail (GSSDP_IS_ROOT_DEVICE (root_device));
        g_return_if_fail (GSSDP_IS_DEVICE (device));
        g_return_if_fail (!GSSDP_IS_ROOT_DEVICE (device));

        root_device->priv->devices = g_list_append (root_device->priv->devices,
                                                    device);

        g_object_notify (G_OBJECT (root_device), "devices");
}

/**
 * Removes @device from the list of devices contained in @root_device
 **/
void
gssdp_root_device_remove_device (GSSDPRootDevice *root_device,
                                 GSSDPDevice     *device)
{
        g_return_if_fail (GSSDP_IS_ROOT_DEVICE (root_device));
        g_return_if_fail (GSSDP_IS_DEVICE (device));
        g_return_if_fail (!GSSDP_IS_ROOT_DEVICE (device));

        root_device->priv->devices = g_list_remove (root_device->priv->devices,
                                                    device);

        g_object_notify (G_OBJECT (root_device), "devices");
}

/**
 * gssdp_device_get_devices
 * @root_device: A #GSSDPRootDevice
 *
 * Return value: A #GList of devices contained in @root_device.
 **/
const GList *
gssdp_root_device_get_devices (GSSDPRootDevice *root_device)
{
        g_return_val_if_fail (GSSDP_IS_ROOT_DEVICE (root_device), NULL);
        
        return root_device->priv->devices;
}

/**
 * gssdp_root_device_discover
 * @root_device: A #GSSDPRootDevice
 * @target: The discovery target
 *
 * Sends a discovery request for @target.
 **/
void
gssdp_root_device_discover (GSSDPRootDevice *root_device,
                            const char      *target)
{
        char *request;
        struct sockaddr_in sin;
        int res;

        g_return_if_fail (GSSDP_IS_ROOT_DEVICE (root_device));
        g_return_if_fail (target != NULL);

        request = g_strdup_printf (SSDP_DISCOVERY_REQUEST,
                                   target,
                                   root_device->priv->mx);

        memset (&sin, 0, sizeof (sin));

        sin.sin_family      = AF_INET;
        sin.sin_port        = htons (SSDP_PORT);
        sin.sin_addr.s_addr = inet_addr (SSDP_ADDR);

        res = sendto (root_device->priv->socket_fd,
                      request,
                      strlen (request),
                      0,
                      (struct sockaddr *) &sin,
                      sizeof (struct sockaddr_in));

        if (res == -1)
                emit_error (root_device, errno);

        g_free (request);
}

/**
 * Emits @error_number using the 'error' signal
 **/
static void
emit_error (GSSDPRootDevice *root_device,
            int              error_number)
{
        GError *error;

        error = g_error_new (GSSDP_ERROR_QUARK,
                             error_number,
                             strerror (error_number));

        g_signal_emit (root_device,
                       signals[ERROR],
                       0,
                       error);

        g_error_free (error);
}

/**
 * SocketSource implementation
 **/
static gboolean
socket_source_prepare (GSource *source,
                       int     *timeout)
{
        return FALSE;
}

static gboolean
socket_source_check (GSource *source)
{
        SocketSource *socket_source;

        socket_source = (SocketSource *) source;

        return socket_source->poll_fd.revents & (G_IO_IN | G_IO_ERR);
}

static gboolean
socket_source_dispatch (GSource    *source,
                        GSourceFunc callback,
                        gpointer    user_data)
{
        SocketSource *socket_source;

        socket_source = (SocketSource *) source;

        if (socket_source->poll_fd.revents & G_IO_IN) {
                /* Ready to read */
                ssize_t bytes;
                char buf[1024];

                bytes = recv (socket_source->poll_fd.fd,
                              buf,
                              1024,
                              0);
                buf[bytes] = 0;

                g_print ("received %s\n", buf);
        } else if (socket_source->poll_fd.revents & G_IO_ERR) {
                /* An error occured */
                int value;
                socklen_t size_int;

                value = EINVAL;
                size_int = sizeof (int);
                
                /* Get errno from socket */
                getsockopt (socket_source->poll_fd.fd,
                            SOL_SOCKET,
                            SO_ERROR,
                            &value,
                            &size_int);

                emit_error (socket_source->root_device, value);
        }

        return TRUE;
}
