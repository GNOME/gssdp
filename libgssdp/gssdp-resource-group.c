/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gssdp-resource-group
 * @short_description: Class for controlling resource announcement.
 *
 * A #GSSDPResourceGroup is a group of SSDP resources whose availability can
 * be controlled as one. This is useful when one needs to announce a single
 * service as multiple SSDP resources (UPnP does this for example).
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <libsoup/soup.h>

#include "gssdp-resource-group.h"
#include "gssdp-resource-browser.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"

G_DEFINE_TYPE (GSSDPResourceGroup,
               gssdp_resource_group,
               G_TYPE_OBJECT);

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
        resource_group->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (resource_group,
                                         GSSDP_TYPE_RESOURCE_GROUP,
                                         GSSDPResourceGroupPrivate);

        resource_group->priv->max_age = SSDP_DEFAULT_MAX_AGE;
        resource_group->priv->message_delay = DEFAULT_MESSAGE_DELAY;

        resource_group->priv->message_queue = g_queue_new ();
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
        priv = resource_group->priv;

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

                g_queue_free (priv->message_queue);
                priv->message_queue = NULL;
        }

        if (priv->message_src) {
                g_source_destroy (priv->message_src);
                priv->message_src = NULL;
        }

        if (priv->timeout_src) {
                g_source_destroy (priv->timeout_src);
                priv->timeout_src = NULL;
        }

        if (priv->client) {
                if (g_signal_handler_is_connected
                        (priv->client,
                         priv->message_received_id)) {
                        g_signal_handler_disconnect
                                (priv->client,
                                 priv->message_received_id);
                }
                                                   
                g_object_unref (priv->client);
                priv->client = NULL;
        }

        G_OBJECT_CLASS (gssdp_resource_group_parent_class)->dispose (object);
}

static void
gssdp_resource_group_class_init (GSSDPResourceGroupClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gssdp_resource_group_set_property;
        object_class->get_property = gssdp_resource_group_get_property;
        object_class->dispose      = gssdp_resource_group_dispose;

        g_type_class_add_private (klass, sizeof (GSSDPResourceGroupPrivate));

        /**
         * GSSDPResourceGroup:client:
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
         * GSSDPResourceGroup:max-age:
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
         * GSSDPResourceGroup:available:
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
         * GSSDPResourceGroup:message-delay:
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
        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        resource_group->priv->client = g_object_ref (client);

        resource_group->priv->message_received_id =
                g_signal_connect_object (resource_group->priv->client,
                                         "message-received",
                                         G_CALLBACK (message_received_cb),
                                         resource_group,
                                         0);

        g_object_notify (G_OBJECT (resource_group), "client");
}

/**
 * gssdp_resource_group_get_client:
 * @resource_group: A #GSSDPResourceGroup
 *
 * Returns: (transfer none): The #GSSDPClient @resource_group is associated with.
 **/
GSSDPClient *
gssdp_resource_group_get_client (GSSDPResourceGroup *resource_group)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), NULL);

        return resource_group->priv->client;
}

/**
 * gssdp_resource_group_set_max_age:
 * @resource_group: A #GSSDPResourceGroup
 * @max_age: The number of seconds advertisements are valid
 *
 * Sets the number of seconds advertisements are valid to @max_age.
 **/
void
gssdp_resource_group_set_max_age (GSSDPResourceGroup *resource_group,
                                  guint               max_age)
{
        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));

        if (resource_group->priv->max_age == max_age)
                return;

        resource_group->priv->max_age = max_age;
        
        g_object_notify (G_OBJECT (resource_group), "max-age");
}

/**
 * gssdp_resource_group_get_max_age:
 * @resource_group: A #GSSDPResourceGroup
 *
 * Return value: The number of seconds advertisements are valid.
 **/
guint
gssdp_resource_group_get_max_age (GSSDPResourceGroup *resource_group)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), 0);

        return resource_group->priv->max_age;
}

/**
 * gssdp_resource_group_set_message_delay:
 * @resource_group: A #GSSDPResourceGroup
 * @message_delay: The message delay in ms.
 *
 * Sets the minimum time between each SSDP message.
 **/
void
gssdp_resource_group_set_message_delay (GSSDPResourceGroup *resource_group,
                                        guint               message_delay)
{
        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));

        if (resource_group->priv->message_delay == message_delay)
                return;

        resource_group->priv->message_delay = message_delay;
        
        g_object_notify (G_OBJECT (resource_group), "message-delay");
}

/**
 * gssdp_resource_group_get_message_delay:
 * @resource_group: A #GSSDPResourceGroup
 *
 * Return value: the minimum time between each SSDP message in ms.
 **/
guint
gssdp_resource_group_get_message_delay (GSSDPResourceGroup *resource_group)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), 0);

        return resource_group->priv->message_delay;
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
send_announcement_set (GList *resources, GFunc message_function)
{
        guint8 i;

        for (i = 0; i < DEFAULT_ANNOUNCEMENT_SET_SIZE; i++) {
                g_list_foreach (resources, message_function, NULL);
        }
}

/**
 * gssdp_resource_group_set_available:
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
        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));

        if (resource_group->priv->available == available)
                return;

        resource_group->priv->available = available;

        if (available) {
                int timeout;

                /* We want to re-announce at least 3 times before the resource
                 * group expires to cope with the unrelialble nature of UDP.
                 *
                 * Read the paragraphs about 'CACHE-CONTROL' on pages 21-22 of
                 * UPnP Device Architecture Document v1.1 for further details.
                 * */
                timeout = resource_group->priv->max_age;
                if (G_LIKELY (timeout > 6))
                        timeout = (timeout / 3) - 1;

                /* Add re-announcement timer */
                resource_group->priv->timeout_src =
                        g_timeout_source_new_seconds (timeout);
                g_source_set_callback (resource_group->priv->timeout_src,
                                       resource_group_timeout,
                                       resource_group, NULL);

                g_source_attach (resource_group->priv->timeout_src,
                                 g_main_context_get_thread_default ());

                g_source_unref (resource_group->priv->timeout_src);

                /* Make sure initial byebyes are sent grouped before initial
                 * alives */
                send_announcement_set (resource_group->priv->resources,
                                       (GFunc) send_initial_resource_byebye);

                send_announcement_set (resource_group->priv->resources,
                                       (GFunc) resource_alive);
        } else {
                /* Unannounce all resources */
                send_announcement_set (resource_group->priv->resources,
                                       (GFunc) resource_byebye);

                /* Remove re-announcement timer */
                g_source_destroy (resource_group->priv->timeout_src);
                resource_group->priv->timeout_src = NULL;
        }
        
        g_object_notify (G_OBJECT (resource_group), "available");
}

/**
 * gssdp_resource_group_get_available:
 * @resource_group: A #GSSDPResourceGroup
 *
 * Return value: TRUE if @resource_group is available (advertised).
 **/
gboolean
gssdp_resource_group_get_available (GSSDPResourceGroup *resource_group)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), FALSE);

        return resource_group->priv->available;
}

/**
 * gssdp_resource_group_add_resource:
 * @resource_group: A #GSSDPResourceGroup
 * @target: The resource's target
 * @usn: The resource's USN
 * @locations: (element-type utf8): A #GList of the resource's locations
 *
 * Adds a resource with target @target, USN @usn, and locations @locations
 * to @resource_group.
 *
 * Return value: The ID of the added resource.
 **/
guint
gssdp_resource_group_add_resource (GSSDPResourceGroup *resource_group,
                                   const char         *target,
                                   const char         *usn,
                                   GList              *locations)
{
        Resource *resource;
        GList *l;
        GError *error;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), 0);
        g_return_val_if_fail (target != NULL, 0);
        g_return_val_if_fail (usn != NULL, 0);
        g_return_val_if_fail (locations != NULL, 0);

        resource = g_slice_new0 (Resource);

        resource->resource_group = resource_group;

        resource->target = g_strdup (target);
        resource->usn    = g_strdup (usn);

        error = NULL;
        resource->target_regex = create_target_regex (target, &resource->version, &error);
        if (error) {
                g_warning ("Error compiling regular expression for '%s': %s",
                           target,
                           error->message);

                g_error_free (error);
                resource_free (resource);

                return 0;
        }

        resource->initial_byebye_sent = FALSE;

        for (l = locations; l; l = l->next) {
                resource->locations = g_list_append (resource->locations,
                                                    g_strdup (l->data));
        }

        resource_group->priv->resources =
                g_list_prepend (resource_group->priv->resources, resource);

        resource->id = ++resource_group->priv->last_resource_id;

        if (resource_group->priv->available)
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
 * to @resource_group.
 *
 * Return value: The ID of the added resource.
 **/
guint
gssdp_resource_group_add_resource_simple (GSSDPResourceGroup *resource_group,
                                          const char         *target,
                                          const char         *usn,
                                          const char         *location)
{
        GList *locations = NULL;
        guint  resource_id;

        locations = g_list_append (locations, (gpointer) location);
        resource_id = gssdp_resource_group_add_resource (resource_group, target, usn, locations);

        g_list_free (locations);

        return resource_id;
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
        GList *l;

        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));
        g_return_if_fail (resource_id > 0);

        for (l = resource_group->priv->resources; l; l = l->next) {
                Resource *resource;

                resource = l->data;

                if (resource->id == resource_id) {
                        resource_group->priv->resources = 
                                g_list_remove (resource_group->priv->resources,
                                               resource);
                        
                        resource_free (resource);

                        return;
                }
        }
}

/*
 * Called to re-announce all resources periodically
 */
static gboolean
resource_group_timeout (gpointer user_data)
{
        GSSDPResourceGroup *resource_group;

        resource_group = GSSDP_RESOURCE_GROUP (user_data);

        send_announcement_set (resource_group->priv->resources,
                               (GFunc) resource_alive);

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
        const char *target, *mx_str, *version_str, *man;
        gboolean want_all;
        int mx, version;
        GList *l;

        resource_group = GSSDP_RESOURCE_GROUP (user_data);

        /* Only process if we are available */
        if (!resource_group->priv->available)
                return;

        /* We only handle discovery requests */
        if (type != _GSSDP_DISCOVERY_REQUEST)
                return;

        /* Extract target */
        target = soup_message_headers_get_one (headers, "ST");
        if (!target) {
                g_warning ("Discovery request did not have an ST header");

                return;
        }

        /* Is this the "ssdp:all" target? */
        want_all = (strcmp (target, GSSDP_ALL_RESOURCES) == 0);

        /* Extract MX */
        mx_str = soup_message_headers_get_one (headers, "MX");
        if (!mx_str || atoi (mx_str) <= 0) {
                g_warning ("Discovery request did not have a valid MX header");

                return;
        }

        man = soup_message_headers_get_one (headers, "MAN");
        if (!man || strcmp (man, DEFAULT_MAN_HEADER) != 0) {
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
        for (l = resource_group->priv->resources; l; l = l->next) {
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
       if (resource->locations->next) {
                GString *al_string;
                GList *l;

                al_string = g_string_new ("AL: ");

                for (l = resource->locations->next; l; l = l->next) {
                        g_string_append_c (al_string, '<');
                        g_string_append (al_string, l->data);
                        g_string_append_c (al_string, '>');
                }

                g_string_append (al_string, "\r\n");

                return g_string_free (al_string, FALSE);
        } else
                return NULL; 
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
        DiscoveryResponse *response;
        GSSDPClient *client;
        SoupDate *date;
        char *al, *date_str, *message;
        guint max_age;
        char *usn;

        response = user_data;

        /* Send message */
        client = response->resource->resource_group->priv->client;

        max_age = response->resource->resource_group->priv->max_age;

        al = construct_al (response->resource);
        usn = construct_usn (response->resource->usn,
                             response->target,
                             response->resource->target);
        date = soup_date_new_from_now (0);
        date_str = soup_date_to_string (date, SOUP_DATE_HTTP);
        soup_date_free (date);

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

        resource_group = GSSDP_RESOURCE_GROUP (data);

        if (g_queue_is_empty (resource_group->priv->message_queue)) {
                /* this is the timeout after last message in queue */
                resource_group->priv->message_src = NULL;

                return FALSE;
        } else {
                GSSDPClient *client;
                char *message;

                client = resource_group->priv->client;
                message = g_queue_pop_head
                        (resource_group->priv->message_queue);

                _gssdp_client_send_message (client,
                                            NULL,
                                            0,
                                            message,
                                            _GSSDP_DISCOVERY_RESPONSE);
                g_free (message);

                return TRUE;
        }
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
        g_queue_push_tail (resource_group->priv->message_queue, 
                           message);

        if (resource_group->priv->message_src == NULL) {
                /* nothing in the queue: process message immediately 
                   and add a timeout for (possible) next message */
                process_queue (resource_group);
                resource_group->priv->message_src = g_timeout_source_new (
                    resource_group->priv->message_delay);
                g_source_set_callback (resource_group->priv->message_src,
                    process_queue, resource_group, NULL);
                g_source_attach (resource_group->priv->message_src,
                                 g_main_context_get_thread_default ());
                g_source_unref (resource_group->priv->message_src);
        }
}

/*
 * Send ssdp:alive message for @resource
 */
static void
resource_alive (Resource *resource)
{
        GSSDPClient *client;
        guint max_age;
        char *al, *message;
        const char *group;
        char *dest;

        /* Send initial byebye if not sent already */
        send_initial_resource_byebye (resource);

        /* Send message */
        client = resource->resource_group->priv->client;

        max_age = resource->resource_group->priv->max_age;

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
        GSSDPClient *client = NULL;

        client = resource->resource_group->priv->client;

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
        while (resource->responses)
                discovery_response_free (resource->responses->data);

        if (resource->resource_group->priv->available)
                resource_byebye (resource);

        g_free (resource->usn);
        g_free (resource->target);

        if (resource->target_regex)
                g_regex_unref (resource->target_regex);

        while (resource->locations) {
                g_free (resource->locations->data);
                resource->locations = g_list_delete_link (resource->locations,
                                                         resource->locations);
        }

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
                *version = atoi (version_str);
                strcpy (version_str, VERSION_PATTERN);
        }

        regex = g_regex_new (pattern, 0, 0, error);

        g_free (pattern);

        return regex;
}

