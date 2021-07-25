/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>

#include "gssdp-resource-group.h"
#include "gssdp-resource-browser.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <libsoup/soup.h>

#define DEFAULT_MAN_HEADER "\"ssdp:discover\""

struct _GSSDPResourceGroupPrivate {
        GSSDPClient *client;

        guint        max_age;

        gboolean     available;

        GList       *resources;

        gulong       message_received_id;

        GSource     *timeout_src;

        guint        last_resource_id;
        
        guint        message_delay;
        GQueue      *message_queue;
        GSource     *message_src;
};
typedef struct _GSSDPResourceGroupPrivate GSSDPResourceGroupPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GSSDPResourceGroup,
                            gssdp_resource_group,
                            G_TYPE_OBJECT)

enum {
        PROP_0,
        PROP_CLIENT,
        PROP_MAX_AGE,
        PROP_AVAILABLE,
        PROP_MESSAGE_DELAY
};

typedef struct {
        GSSDPResourceGroup *resource_group;

        GRegex              *target_regex;
        char                *target;
        char                *usn;
        GList               *locations;

        GList               *responses;

        guint                id;

        guint                version;

        gboolean             initial_byebye_sent;
} Resource;

typedef struct {
        char     *dest_ip;
        gushort   dest_port;
        char     *target;
        Resource *resource;

        GSource  *timeout_src;
} DiscoveryResponse;

#define DEFAULT_MESSAGE_DELAY 120
#define DEFAULT_ANNOUNCEMENT_SET_SIZE 3
#define VERSION_PATTERN "[0-9]+$"

/* Function prototypes */

static void
queue_message                   (GSSDPResourceGroup *resource_group,
                                 char               *message);
static void
gssdp_resource_group_set_client (GSSDPResourceGroup *resource_group,
                                 GSSDPClient        *client);
static gboolean
resource_group_timeout          (gpointer            user_data);
static void
message_received_cb             (GSSDPClient        *client,
                                 const char         *from_ip,
                                 gushort             from_port,
                                 _GSSDPMessageType   type,
                                 SoupMessageHeaders *headers,
                                 gpointer            user_data);
static void
resource_alive                  (Resource           *resource);
static void
resource_byebye                 (Resource           *resource);
static void
resource_free                   (Resource           *resource);
static gboolean
discovery_response_timeout      (gpointer            user_data);
static void
discovery_response_free         (DiscoveryResponse  *response);
static gboolean
process_queue                   (gpointer            data);
static char *
get_version_for_target          (char *target);
static GRegex *
create_target_regex             (const char         *target,
                                 guint              *version,
                                 GError            **error);
static void
send_initial_resource_byebye    (Resource          *resource);

static void
gssdp_resource_group_init (GSSDPResourceGroup *resource_group)
{
        GSSDPResourceGroupPrivate *priv;

        priv = gssdp_resource_group_get_instance_private (resource_group);

        priv->max_age = SSDP_DEFAULT_MAX_AGE;
        priv->message_delay = DEFAULT_MESSAGE_DELAY;

        priv->message_queue = g_queue_new ();
}

static void
gssdp_resource_group_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        GSSDPResourceGroup *resource_group;

        resource_group = GSSDP_RESOURCE_GROUP (object);

        switch (property_id) {
        case PROP_CLIENT:
                g_value_set_object
                        (value,
                         gssdp_resource_group_get_client (resource_group));
                break;
        case PROP_MAX_AGE:
                g_value_set_uint
                        (value,
                         gssdp_resource_group_get_max_age (resource_group));
                break;
        case PROP_AVAILABLE:
                g_value_set_boolean
                        (value,
                         gssdp_resource_group_get_available (resource_group));
                break;
        case PROP_MESSAGE_DELAY:
                g_value_set_uint
                        (value,
                         gssdp_resource_group_get_message_delay 
                                (resource_group));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_resource_group_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
        GSSDPResourceGroup *resource_group;

        resource_group = GSSDP_RESOURCE_GROUP (object);

        switch (property_id) {
        case PROP_CLIENT:
                gssdp_resource_group_set_client (resource_group,
                                                 g_value_get_object (value));
                break;
        case PROP_MAX_AGE:
                gssdp_resource_group_set_max_age (resource_group,
                                                  g_value_get_uint (value));
                break;
        case PROP_AVAILABLE:
                gssdp_resource_group_set_available
                        (resource_group, g_value_get_boolean (value));
                break;
        case PROP_MESSAGE_DELAY:
                gssdp_resource_group_set_message_delay
                        (resource_group, g_value_get_uint (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_resource_group_dispose (GObject *object)
{
        GSSDPResourceGroup *resource_group;
        GSSDPResourceGroupPrivate *priv;

        resource_group = GSSDP_RESOURCE_GROUP (object);
        priv = gssdp_resource_group_get_instance_private (resource_group);

        g_list_free_full (priv->resources, (GFreeFunc) resource_free);
        priv->resources = NULL;

        if (priv->message_queue) {
                /* send messages without usual delay */
                while (!g_queue_is_empty (priv->message_queue)) {
                        if (priv->available)
                                process_queue (resource_group);
                        else
                                g_free (g_queue_pop_head
                                        (priv->message_queue));
                }

                g_clear_pointer (&priv->message_queue, g_queue_free);
        }


        /* No need to unref sources, already done on creation */
        g_clear_pointer (&priv->message_src, g_source_destroy);
        g_clear_pointer (&priv->timeout_src, g_source_destroy);

        if (priv->client) {
                if (g_signal_handler_is_connected
                        (priv->client,
                         priv->message_received_id)) {
                        g_signal_handler_disconnect
                                (priv->client,
                                 priv->message_received_id);
                }

                g_clear_object (&priv->client);
        }

        G_OBJECT_CLASS (gssdp_resource_group_parent_class)->dispose (object);
}

/**
 * GSSDPResourceGroup:
 *
 * Class for controlling resource announcement.
 *
 * A #GSSDPResourceGroup is a group of SSDP resources whose availability can
 * be controlled as one. This is useful when one needs to announce a single
 * service as multiple SSDP resources (UPnP does this for example).
 */

static void
gssdp_resource_group_class_init (GSSDPResourceGroupClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gssdp_resource_group_set_property;
        object_class->get_property = gssdp_resource_group_get_property;
        object_class->dispose      = gssdp_resource_group_dispose;

        /**
         * GSSDPResourceGroup:client:(attributes org.gtk.Property.get=gssdp_resource_group_get_client):
         *
         * The #GSSDPClient to use.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_CLIENT,
                 g_param_spec_object
                         ("client",
                          "Client",
                          "The associated client.",
                          GSSDP_TYPE_CLIENT,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPResourceGroup:max-age:(attributes org.gtk.Property.set=gssdp_resource_group_set_max_age org.gtk.Property.get=gssdp_resource_group_get_max_age ):
         *
         * The number of seconds our advertisements are valid.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_MAX_AGE,
                 g_param_spec_uint
                         ("max-age",
                          "Max age",
                          "The number of seconds advertisements are valid.",
                          0,
                          G_MAXUINT,
                          SSDP_DEFAULT_MAX_AGE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPResourceGroup:available:(attributes org.gtk.Property.set=gssdp_resource_group_set_available org.gtk.Property.get=gssdp_resource_group_get_available ):
         *
         * Whether this group of resources is available or not.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_AVAILABLE,
                 g_param_spec_boolean
                         ("available",
                          "Available",
                          "Whether this group of resources is available or "
                          "not.",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPResourceGroup:message-delay:(attributes org.gtk.Property.set=gssdp_resource_group_set_message_delay org.gtk.Property.get=gssdp_resource_group_get_message_delay ):
         *
         * The minimum number of milliseconds between SSDP messages.
         * The default is 120 based on DLNA specification.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_MESSAGE_DELAY,
                 g_param_spec_uint
                         ("message-delay",
                          "Message delay",
                          "The minimum number of milliseconds between SSDP "
                          "messages.",
                          0,
                          G_MAXUINT,
                          DEFAULT_MESSAGE_DELAY,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));
}

/**
 * gssdp_resource_group_new:
 * @client: The #GSSDPClient to associate with
 *
 * Return value: A new #GSSDPResourceGroup object.
 **/
GSSDPResourceGroup *
gssdp_resource_group_new (GSSDPClient *client)
{
        return g_object_new (GSSDP_TYPE_RESOURCE_GROUP,
                             "client", client,
                             NULL);
}

/*
 * Sets the #GSSDPClient @resource_group is associated with @client
 */
static void
gssdp_resource_group_set_client (GSSDPResourceGroup *resource_group,
                                 GSSDPClient        *client)
{
        GSSDPResourceGroupPrivate *priv;

        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        priv = gssdp_resource_group_get_instance_private (resource_group);
        priv->client = g_object_ref (client);

        priv->message_received_id =
                g_signal_connect_object (priv->client,
                                         "message-received",
                                         G_CALLBACK (message_received_cb),
                                         resource_group,
                                         0);

        g_object_notify (G_OBJECT (resource_group), "client");
}

/**
 * gssdp_resource_group_get_client:(attributes org.gtk.Method.get_property=client):
 * @resource_group: A #GSSDPResourceGroup
 *
 * Returns: (transfer none): The #GSSDPClient @resource_group is associated with.
 **/
GSSDPClient *
gssdp_resource_group_get_client (GSSDPResourceGroup *resource_group)
{
        GSSDPResourceGroupPrivate *priv;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), NULL);

        priv = gssdp_resource_group_get_instance_private (resource_group);

        return priv->client;
}

/**
 * gssdp_resource_group_set_max_age:(attributes org.gtk.Method.set_property=max-age):
 * @resource_group: A #GSSDPResourceGroup
 * @max_age: The number of seconds advertisements are valid
 *
 * Sets the number of seconds advertisements are valid to @max_age.
 **/
void
gssdp_resource_group_set_max_age (GSSDPResourceGroup *resource_group,
                                  guint               max_age)
{
        GSSDPResourceGroupPrivate *priv;

        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));

        priv = gssdp_resource_group_get_instance_private (resource_group);
        if (priv->max_age == max_age)
                return;

        priv->max_age = max_age;

        g_object_notify (G_OBJECT (resource_group), "max-age");
}

/**
 * gssdp_resource_group_get_max_age:(attributes org.gtk.Method.get_property=max-age):
 * @resource_group: A #GSSDPResourceGroup
 *
 * Return value: The number of seconds advertisements are valid.
 **/
guint
gssdp_resource_group_get_max_age (GSSDPResourceGroup *resource_group)
{
        GSSDPResourceGroupPrivate *priv;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), 0);
        priv = gssdp_resource_group_get_instance_private (resource_group);

        return priv->max_age;
}

/**
 * gssdp_resource_group_set_message_delay:(attributes org.gtk.Method.set_property=message-delay):
 * @resource_group: A #GSSDPResourceGroup
 * @message_delay: The message delay in ms.
 *
 * Sets the minimum time between each SSDP message.
 **/
void
gssdp_resource_group_set_message_delay (GSSDPResourceGroup *resource_group,
                                        guint               message_delay)
{
        GSSDPResourceGroupPrivate *priv;

        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));

        priv = gssdp_resource_group_get_instance_private (resource_group);
        if (priv->message_delay == message_delay)
                return;

        priv->message_delay = message_delay;

        g_object_notify (G_OBJECT (resource_group), "message-delay");
}

/**
 * gssdp_resource_group_get_message_delay:(attributes org.gtk.Method.get_property=message-delay):
 * @resource_group: A #GSSDPResourceGroup
 *
 * Return value: the minimum time between each SSDP message in ms.
 **/
guint
gssdp_resource_group_get_message_delay (GSSDPResourceGroup *resource_group)
{
        GSSDPResourceGroupPrivate *priv;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), 0);
        priv = gssdp_resource_group_get_instance_private (resource_group);

        return priv->message_delay;
}

static void
send_initial_resource_byebye (Resource *resource)
{
        if (!resource->initial_byebye_sent) {
                /* Unannounce before first announce. This is
                   done to minimize the possibility of
                   control points thinking that this is just
                   a reannouncement. */
                resource_byebye (resource);

                resource->initial_byebye_sent = TRUE;
        }
}

static void
send_announcement_set (GList *resources, GFunc message_function, gpointer user_data)
{
        guint8 i;

        for (i = 0; i < DEFAULT_ANNOUNCEMENT_SET_SIZE; i++) {
                g_list_foreach (resources, message_function, user_data);
        }
}

static void
setup_reannouncement_timeout (GSSDPResourceGroup *resource_group)
{
        int timeout;
        GSSDPResourceGroupPrivate *priv;

        priv = gssdp_resource_group_get_instance_private (resource_group);

        /* We want to re-announce at least 3 times before the resource
         * group expires to cope with the unrelialble nature of UDP.
         *
         * Read the paragraphs about 'CACHE-CONTROL' on pages 21-22 of
         * UPnP Device Architecture Document v1.1 for further details.
         * */
        timeout = priv->max_age;
        if (G_LIKELY (timeout > 6))
                timeout = (timeout / 3) - 1;

        /* Add re-announcement timer */
        priv->timeout_src = g_timeout_source_new_seconds (timeout);
        g_source_set_callback (priv->timeout_src,
                        resource_group_timeout,
                        resource_group, NULL);

        g_source_attach (priv->timeout_src,
                        g_main_context_get_thread_default ());

        g_source_unref (priv->timeout_src);
}

/**
 * gssdp_resource_group_set_available:(attributes org.gtk.Method.set_property=available):
 * @resource_group: A #GSSDPResourceGroup
 * @available: %TRUE if @resource_group should be available (advertised)
 *
 * Sets @resource_group<!-- -->s availability to @available. Changing
 * @resource_group<!-- -->s availability causes it to announce its new state
 * to listening SSDP clients.
 **/
void
gssdp_resource_group_set_available (GSSDPResourceGroup *resource_group,
                                    gboolean            available)
{
        GSSDPResourceGroupPrivate *priv;

        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));

        priv = gssdp_resource_group_get_instance_private (resource_group);
        if (priv->available == available)
                return;

        priv->available = available;

        if (available) {
                setup_reannouncement_timeout (resource_group);
                /* Make sure initial byebyes are sent grouped before initial
                 * alives */
                send_announcement_set (priv->resources,
                                       (GFunc) send_initial_resource_byebye,
                                       NULL);

                send_announcement_set (priv->resources,
                                       (GFunc) resource_alive,
                                       NULL);
        } else {
                /* Unannounce all resources */
                send_announcement_set (priv->resources,
                                       (GFunc) resource_byebye,
                                       NULL);

                /* Remove re-announcement timer */
                g_source_destroy (priv->timeout_src);
                priv->timeout_src = NULL;
        }

        g_object_notify (G_OBJECT (resource_group), "available");
}

/**
 * gssdp_resource_group_get_available:(attributes org.gtk.Method.get_property=available):
 * @resource_group: A #GSSDPResourceGroup
 *
 * Return value: TRUE if @resource_group is available (advertised).
 **/
gboolean
gssdp_resource_group_get_available (GSSDPResourceGroup *resource_group)
{
        GSSDPResourceGroupPrivate *priv;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), FALSE);
        priv = gssdp_resource_group_get_instance_private (resource_group);

        return priv->available;
}

/**
 * gssdp_resource_group_add_resource:
 * @resource_group: A #GSSDPResourceGroup
 * @target: The resource's target
 * @usn: The resource's USN
 * @locations: (element-type utf8)(transfer none): A #GList of the resource's locations
 *
 * Add an additional resource to announce in this resource group.
 *
 * Adds a resource with target @target, USN @usn, and locations @locations
 * to @resource_group. If the resource group is set [property@GSSDP.ResourceGroup:available],
 * it will be announced right away.
 *
 * If your resource only has one location, you can use [method@GSSDP.ResourceGroup.add_resource_simple]
 * instead.
 *
 * The resource id that is returned by this function can be used with
 * [method@GSSDP.ResourceGroup.remove_resource].
 *
 * Return value: The ID of the added resource.
 **/
guint
gssdp_resource_group_add_resource (GSSDPResourceGroup *resource_group,
                                   const char         *target,
                                   const char         *usn,
                                   GList              *locations)
{
        GSSDPResourceGroupPrivate *priv = NULL;
        Resource *resource = NULL;
        GError *error = NULL;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), 0);
        g_return_val_if_fail (target != NULL, 0);
        g_return_val_if_fail (usn != NULL, 0);
        g_return_val_if_fail (locations != NULL, 0);

        priv = gssdp_resource_group_get_instance_private (resource_group);

        resource = g_slice_new0 (Resource);

        resource->resource_group = resource_group;

        resource->target = g_strdup (target);
        resource->usn    = g_strdup (usn);

        resource->target_regex = create_target_regex (target,
                                                      &resource->version,
                                                      &error);
        if (error) {
                g_warning ("Error compiling regular expression for '%s': %s",
                           target,
                           error->message);

                g_error_free (error);
                resource_free (resource);

                return 0;
        }

        resource->initial_byebye_sent = FALSE;

        resource->locations = g_list_copy_deep (locations, (GCopyFunc) g_strdup, NULL);

        priv->resources = g_list_prepend (priv->resources, resource);

        resource->id = ++priv->last_resource_id;

        if (priv->available)
                resource_alive (resource);

        return resource->id;
}

/**
 * gssdp_resource_group_add_resource_simple:
 * @resource_group: A #GSSDPResourceGroup
 * @target: The resource's target
 * @usn: The resource's USN
 * @location: The resource's location
 *
 * Adds a resource with target @target, USN @usn, and location @location
 * to @resource_group. If the resource group is set [property@GSSDP.ResourceGroup:available],
 * it will be announced right away.
 *
 * The resource id that is returned by this function can be used with
 * [method@GSSDP.ResourceGroup.remove_resource].

 * Return value: The ID of the added resource.
 **/
guint
gssdp_resource_group_add_resource_simple (GSSDPResourceGroup *resource_group,
                                          const char         *target,
                                          const char         *usn,
                                          const char         *location)
{
        GList list = { 0 };
        list.data = (gpointer)location;

        return gssdp_resource_group_add_resource (resource_group,
                                                  target,
                                                  usn,
                                                  &list);
}

/**
 * gssdp_resource_group_remove_resource:
 * @resource_group: A #GSSDPResourceGroup
 * @resource_id: The ID of the resource to remove
 *
 * Removes the resource with ID @resource_id from @resource_group.
 **/
void
gssdp_resource_group_remove_resource (GSSDPResourceGroup *resource_group,
                                      guint               resource_id)
{
        GSSDPResourceGroupPrivate *priv;
        GList *l;

        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));
        g_return_if_fail (resource_id > 0);

        priv = gssdp_resource_group_get_instance_private (resource_group);
        for (l = priv->resources; l; l = l->next) {
                Resource *resource;

                resource = l->data;

                if (resource->id == resource_id) {
                        priv->resources = g_list_remove (priv->resources,
                                                         resource);

                        resource_free (resource);

                        return;
                }
        }
}

static void
resource_update (Resource *resource, gpointer user_data)
{
        GSSDPResourceGroupPrivate *priv;
        GSSDPClient *client;
        char *message;
        const char *group;
        char *dest;
        guint next_boot_id = GPOINTER_TO_UINT (user_data);

        priv = gssdp_resource_group_get_instance_private
                                        (resource->resource_group);

        /* Send message */
        client = priv->client;

        /* FIXME: UGLY V6 stuff */
        group = _gssdp_client_get_mcast_group (client);
        if (strchr (group, ':') != NULL)
                dest = g_strdup_printf ("[%s]", group);
        else
                dest = g_strdup (group);

        message = g_strdup_printf (SSDP_UPDATE_MESSAGE,
                                   dest,
                                   (char *) resource->locations->data,
                                   resource->target,
                                   resource->usn,
                                   next_boot_id);

        queue_message (resource->resource_group, message);

        g_free (dest);
}

/**
 * gssdp_resource_group_update:
 * @resource_group: A #GSSDPResourceGroup
 * @new_boot_id: The new boot id of the device
 *
 * Send an `ssdp::update` message if the underlying `GSSDPClient` is running
 * the UDA 1.1 protocol. Does nothing otherwise.
 *
 * Since: 1.2.0
 */
void
gssdp_resource_group_update (GSSDPResourceGroup *self,
                             guint               next_boot_id)
{
        GSSDPUDAVersion version;
        GSSDPResourceGroupPrivate *priv;

        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (self));
        g_return_if_fail (next_boot_id <= G_MAXINT32);

        priv = gssdp_resource_group_get_instance_private (self);

        version = gssdp_client_get_uda_version (priv->client);

        if (version == GSSDP_UDA_VERSION_1_0)
                return;

        if (!priv->available) {
                gssdp_client_set_boot_id (priv->client, next_boot_id);

                return;
        }

        /* Disable timeout */
        g_clear_pointer (&priv->timeout_src, g_source_destroy);

        send_announcement_set (priv->resources, (GFunc) resource_update, GUINT_TO_POINTER (next_boot_id));

        /* FIXME: This causes only the first of the three update messages to be correct. The other two will
         * have the new boot id as and boot id as the same value
         */
        gssdp_client_set_boot_id (priv->client, next_boot_id);

        setup_reannouncement_timeout (self);
        send_announcement_set (priv->resources, (GFunc) resource_alive, NULL);
}

/*
 * Called to re-announce all resources periodically
 */
static gboolean
resource_group_timeout (gpointer user_data)
{
        GSSDPResourceGroup *resource_group;
        GSSDPResourceGroupPrivate *priv;

        resource_group = GSSDP_RESOURCE_GROUP (user_data);
        priv = gssdp_resource_group_get_instance_private (resource_group);

        send_announcement_set (priv->resources, (GFunc) resource_alive, NULL);

        return TRUE;
}

/*
 * Received a message
 */
static void
message_received_cb (G_GNUC_UNUSED GSSDPClient *client,
                     const char                *from_ip,
                     gushort                    from_port,
                     _GSSDPMessageType          type,
                     SoupMessageHeaders        *headers,
                     gpointer                   user_data)
{
        GSSDPResourceGroup *resource_group;
        GSSDPResourceGroupPrivate *priv;
        const char *target, *mx_str, *version_str, *man;
        gboolean want_all;
        int mx, version;
        GList *l;

        resource_group = GSSDP_RESOURCE_GROUP (user_data);
        priv = gssdp_resource_group_get_instance_private (resource_group);

        /* Only process if we are available */
        if (!priv->available)
                return;

        /* We only handle discovery requests */
        if (type != _GSSDP_DISCOVERY_REQUEST)
                return;

        /* Extract target */
        target = soup_message_headers_get_one (headers, "ST");
        if (target == NULL) {
                g_warning ("Discovery request did not have an ST header");

                return;
        }

        /* Is this the "ssdp:all" target? */
        want_all = (strcmp (target, GSSDP_ALL_RESOURCES) == 0);

        /* Extract MX */
        mx_str = soup_message_headers_get_one (headers, "MX");
        if (mx_str == NULL || atoi (mx_str) <= 0) {
                g_warning ("Discovery request did not have a valid MX header");

                return;
        }

        man = soup_message_headers_get_one (headers, "MAN");
        if (man == NULL || strcmp (man, DEFAULT_MAN_HEADER) != 0) {
                g_warning ("Discovery request did not have a valid MAN header");

                return;
        }

        mx = atoi (mx_str);

        /* Extract version */
        version_str = get_version_for_target ((char *) target);
        if (version_str != NULL)
                version = atoi (version_str);
        else
                version = 0;

        /* Find matching resource */
        for (l = priv->resources; l != NULL; l = l->next) {
                Resource *resource;

                resource = l->data;

                if (want_all ||
                    (g_regex_match (resource->target_regex,
                                    target,
                                    0,
                                    NULL) &&
                     (guint) version <= resource->version)) {
                        /* Match. */
                        guint timeout;
                        DiscoveryResponse *response;

                        /* Get a random timeout from the interval [0, mx] */
                        timeout = g_random_int_range (0, mx * 1000);

                        /* Prepare response */
                        response = g_slice_new (DiscoveryResponse);

                        response->dest_ip   = g_strdup (from_ip);
                        response->dest_port = from_port;
                        response->resource  = resource;

                        if (want_all)
                                response->target = g_strdup (resource->target);
                        else
                                response->target = g_strdup (target);

                        /* Add timeout */
                        response->timeout_src = g_timeout_source_new (timeout);
                        g_source_set_callback (response->timeout_src,
                                               discovery_response_timeout,
                                               response, NULL);

                        g_source_attach (response->timeout_src,
                                         g_main_context_get_thread_default ());

                        g_source_unref (response->timeout_src);

                        /* Add to resource */
                        resource->responses =
                                g_list_prepend (resource->responses, response);
                }
        }
}

/*
 * Construct the AL (Alternative Locations) header for @resource
 */
static char *
construct_al (Resource *resource)
{
        GString *al_string;
        GList *l;

        if (resource->locations->next == NULL) {
                return NULL;
        }

        al_string = g_string_new ("AL: ");

        for (l = resource->locations->next; l; l = l->next) {
                g_string_append_c (al_string, '<');
                g_string_append (al_string, l->data);
                g_string_append_c (al_string, '>');
        }

        g_string_append (al_string, "\r\n");

        return g_string_free (al_string, FALSE);
}

static char *
construct_usn (const char *usn,
               const char *response_target,
               const char *resource_target)
{
        const char *needle;
        char *prefix;
        char *st;

        needle = strstr (usn, resource_target);
        if (needle == NULL)
                return g_strdup (usn);

        prefix = g_strndup (usn, needle - usn);
        st = g_strconcat (prefix, response_target, NULL);

        g_free (prefix);

        return st;
}

/*
 * Send a discovery response
 */
static gboolean
discovery_response_timeout (gpointer user_data)
{
        DiscoveryResponse *response = user_data;
        GSSDPClient *client;
        char *al, *date_str, *message;
        guint max_age;
        char *usn;
        GSSDPResourceGroup *self = response->resource->resource_group;
        GSSDPResourceGroupPrivate *priv;

        priv = gssdp_resource_group_get_instance_private (self);

        /* Send message */
        client = priv->client;

        max_age = priv->max_age;

        al = construct_al (response->resource);
        usn = construct_usn (response->resource->usn,
                             response->target,
                             response->resource->target);
        GDateTime *date = g_date_time_new_now_local ();
        date_str = soup_date_time_to_string (date, SOUP_DATE_HTTP);
        g_date_time_unref (date);

        message = g_strdup_printf (SSDP_DISCOVERY_RESPONSE,
                                   (char *) response->resource->locations->data,
                                   al ? al : "",
                                   usn,
                                   gssdp_client_get_server_id (client),
                                   max_age,
                                   response->target,
                                   date_str);

        _gssdp_client_send_message (client,
                                    response->dest_ip,
                                    response->dest_port,
                                    message,
                                    _GSSDP_DISCOVERY_RESPONSE);

        g_free (message);
        g_free (date_str);
        g_free (al);
        g_free (usn);

        discovery_response_free (response);

        return FALSE;
}

/*
 * Free a DiscoveryResponse structure and its contained data
 */
static void
discovery_response_free (DiscoveryResponse *response)
{
        response->resource->responses =
                g_list_remove (response->resource->responses, response);

        g_source_destroy (response->timeout_src);

        g_free (response->dest_ip);
        g_free (response->target);

        g_slice_free (DiscoveryResponse, response);
}

/*
 * Send the next queued message, if any
 */
static gboolean
process_queue (gpointer data)
{
        GSSDPResourceGroup *resource_group;
        GSSDPResourceGroupPrivate *priv;
        GSSDPClient *client;
        char *message;

        resource_group = GSSDP_RESOURCE_GROUP (data);
        priv = gssdp_resource_group_get_instance_private (resource_group);

        if (g_queue_is_empty (priv->message_queue)) {
                /* this is the timeout after last message in queue */
                priv->message_src = NULL;

                return FALSE;
        }

        client = priv->client;
        message = g_queue_pop_head (priv->message_queue);

        _gssdp_client_send_message (client,
                                    NULL,
                                    0,
                                    message,
                                    _GSSDP_DISCOVERY_RESPONSE);
        g_free (message);

        return TRUE;
}

/*
 * Add a message to sending queue
 * 
 * Do not free @message.
 */
static void
queue_message (GSSDPResourceGroup *resource_group,
               char               *message)
{
        GSSDPResourceGroupPrivate *priv;
        priv = gssdp_resource_group_get_instance_private (resource_group);

        g_queue_push_tail (priv->message_queue, message);

        if (priv->message_src != NULL) {
                return;
        }

        /* nothing in the queue: process message immediately
           and add a timeout for (possible) next message */
        process_queue (resource_group);
        priv->message_src = g_timeout_source_new (priv->message_delay);
        g_source_set_callback (priv->message_src,
                               process_queue,
                               resource_group,
                               NULL);
        g_source_attach (priv->message_src,
                        g_main_context_get_thread_default ());
        g_source_unref (priv->message_src);
}

/*
 * Send ssdp:alive message for @resource
 */
static void
resource_alive (Resource *resource)
{
        GSSDPResourceGroupPrivate *priv;
        GSSDPClient *client;
        guint max_age;
        char *al, *message;
        const char *group;
        char *dest;

        priv = gssdp_resource_group_get_instance_private
                                        (resource->resource_group);

        /* Send initial byebye if not sent already */
        send_initial_resource_byebye (resource);

        /* Send message */
        client = priv->client;

        max_age = priv->max_age;

        al = construct_al (resource);

        /* FIXME: UGLY V6 stuff */
        group = _gssdp_client_get_mcast_group (client);
        if (strchr (group, ':') != NULL)
                dest = g_strdup_printf ("[%s]", group);
        else
                dest = g_strdup (group);

        message = g_strdup_printf (SSDP_ALIVE_MESSAGE,
                                   dest,
                                   max_age,
                                   (char *) resource->locations->data,
                                   al ? al : "",
                                   gssdp_client_get_server_id (client),
                                   resource->target,
                                   resource->usn);

        queue_message (resource->resource_group, message);

        g_free (dest);
        g_free (al);
}

/*
 * Send ssdp:byebye message for @resource
 */
static void
resource_byebye (Resource *resource)
{
        char *message = NULL;
        const char *group = NULL;
        char *host = NULL;
        GSSDPResourceGroupPrivate *priv = NULL;
        GSSDPClient *client = NULL;

        priv = gssdp_resource_group_get_instance_private (resource->resource_group);
        client = priv->client;

        /* FIXME: UGLY V6 stuff */
        group = _gssdp_client_get_mcast_group (client);
        if (strchr (group, ':') != NULL)
                host = g_strdup_printf ("[%s]", group);
        else
                host = g_strdup (group);

        /* Queue message */
        message = g_strdup_printf (SSDP_BYEBYE_MESSAGE,
                                   host,
                                   resource->target,
                                   resource->usn);

        queue_message (resource->resource_group, message);

        g_free (host);
}

/*
 * Free a Resource structure and its contained data
 */
static void
resource_free (Resource *resource)
{
        GSSDPResourceGroupPrivate *priv;

        priv = gssdp_resource_group_get_instance_private
                                        (resource->resource_group);
        /* discovery_response_free will take clear of freeing the list
         * elements and data */
        while (resource->responses)
                discovery_response_free (resource->responses->data);

        if (priv->available)
                resource_byebye (resource);

        g_free (resource->usn);
        g_free (resource->target);

        g_clear_pointer (&resource->target_regex, g_regex_unref);
        g_list_free_full (resource->locations, g_free);

        g_slice_free (Resource, resource);
}

/* Gets you the pointer to the version part in the target string */
static char *
get_version_for_target (char *target)
{
        char *version;

        if (strncmp (target, "urn:", 4) != 0) {
                /* target is not a URN so no version. */
                return NULL;
        }

        // This is ok since there is at least the ":" from URN and the byte after that is the \0
        version = g_strrstr (target, ":") + 1;
        if (version == NULL ||
            !g_regex_match_simple (VERSION_PATTERN, version, 0, 0))
                return NULL;

        return version;
}

static GRegex *
create_target_regex (const char *target, guint *version, GError **error)
{
        GRegex *regex;
        char *pattern;
        char *version_str;

        *version = 0;
        /* Make sure we have enough room for version pattern */
        pattern = g_strndup (target,
                             strlen (target) + strlen (VERSION_PATTERN));

        version_str = get_version_for_target (pattern);
        if (version_str != NULL) {
                /* version_str is now at : + 1 in the pattern string, so this
                 * replaces the actual version with the number matching regex
                 */
                *version = atoi (version_str);
                strcpy (version_str, VERSION_PATTERN);
        }

        regex = g_regex_new (pattern, 0, 0, error);

        g_free (pattern);

        return regex;
}

