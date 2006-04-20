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
#include <string.h>
#include <sys/utsname.h>

#include "gssdp-root-device-private.h"
#include "gssdp-marshal.h"

/* Hack around G_DEFINE_TYPE hardcoding the type function name */
#define gssdp_root_device_get_type gssdp_root_device_type

G_DEFINE_TYPE (GSSDPRootDevice,
               gssdp_root_device,
               GSSDP_TYPE_DEVICE);

#undef gssdp_root_device_get_type

struct _GSSDPRootDevicePrivate {
        char  *location;

        char  *server_id;
        
        GList *devices;
};

enum {
        PROP_0,
        PROP_LOCATION,
        PROP_SERVER_ID,
        PROP_DEVICES
};

enum {
        DISCOVERABLE_AVAILABLE,
        DISCOVERABLE_UNAVAILABLE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Function prototypes */
static void
gssdp_root_device_set_location (GSSDPRootDevice *root_device,
                                const char      *location);

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
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_root_device_notify (GObject    *object,
                          GParamSpec *param_spec)
{
        if (strcmp (param_spec->name, "available") == 0) {
                /* XXX */
        }
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

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gssdp_root_device_set_property;
	object_class->get_property = gssdp_root_device_get_property;
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
        g_return_if_fail (GSSDP_IS_ROOT_DEVICE (root_device));
        g_return_if_fail (target != NULL);

        /* XXX */
}
