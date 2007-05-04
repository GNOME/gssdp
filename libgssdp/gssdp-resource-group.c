/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
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
#include <stdlib.h>

#include "gssdp-resource-group.h"
#include "gssdp-error.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"

G_DEFINE_TYPE (GSSDPResourceGroup,
               gssdp_resource_group,
               G_TYPE_OBJECT);

struct _GSSDPResourceGroupPrivate {
        GSSDPClient *client;

        guint        max_age;

        gboolean     available;

        GList       *resources;

        gulong       message_received_id;

        guint        timeout_id;

        guint        last_resource_id;
};

enum {
        PROP_0,
        PROP_CLIENT,
        PROP_MAX_AGE,
        PROP_AVAILABLE
};

typedef struct {
        GSSDPResourceGroup *resource_group;

        char                *target;
        char                *usn;
        GList               *locations;

        GList               *responses;

        guint                id;
} Resource;

typedef struct {
        char     *dest_ip;
        Resource *resource;

        guint     timeout_id;
} DiscoveryResponse;

/* Function prototypes */
static void
gssdp_resource_group_set_client (GSSDPResourceGroup *resource_group,
                                 GSSDPClient        *client);
static gboolean
resource_group_timeout          (gpointer            user_data);
static void
message_received_cb             (GSSDPClient        *client,
                                 const char         *from_ip,
                                 _GSSDPMessageType   type,
                                 GHashTable         *headers,
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

static void
gssdp_resource_group_init (GSSDPResourceGroup *resource_group)
{
        resource_group->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (resource_group,
                                         GSSDP_TYPE_RESOURCE_GROUP,
                                         GSSDPResourceGroupPrivate);

        resource_group->priv->max_age = SSDP_DEFAULT_MAX_AGE;
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
                                                  g_value_get_long (value));
                break;
        case PROP_AVAILABLE:
                gssdp_resource_group_set_available
                        (resource_group, g_value_get_boolean (value));
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

        resource_group = GSSDP_RESOURCE_GROUP (object);

        if (resource_group->priv->client) {
                if (g_signal_handler_is_connected
                        (resource_group->priv->client,
                         resource_group->priv->message_received_id)) {
                        g_signal_handler_disconnect
                                (resource_group->priv->client,
                                 resource_group->priv->message_received_id);
                }
                                                   
                g_object_unref (resource_group->priv->client);
                resource_group->priv->client = NULL;
        }
        
        while (resource_group->priv->resources) {
                resource_free (resource_group->priv->resources->data);
                resource_group->priv->resources =
                        g_list_delete_link (resource_group->priv->resources,
                                            resource_group->priv->resources);
        }

        if (resource_group->priv->timeout_id > 0) {
                g_source_remove (resource_group->priv->timeout_id);
                resource_group->priv->timeout_id = 0;
        }
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

        g_object_class_install_property
                (object_class,
                 PROP_AVAILABLE,
                 g_param_spec_boolean
                         ("available",
                          "Available",
                          "TRUE if the resources are available (advertised).",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));
}

/**
 * gssdp_resource_group_new
 * @client: The #GSSDPClient to associate with
 * @error: A location to return an error of type #GSSDP_ERROR_QUARK
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

/**
 * Sets the #GSSDPClient @resource_group is associated with @client
 **/
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
 * gssdp_resource_group_get_client
 * @resource_group: A #GSSDPResourceGroup
 *
 * Return value: The #GSSDPClient @resource_group is associated with.
 **/
GSSDPClient *
gssdp_resource_group_get_client (GSSDPResourceGroup *resource_group)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), NULL);

        return resource_group->priv->client;
}

/**
 * gssdp_resource_group_set_max_age
 * @resource_group: A #GSSDPResourceGroup
 * @mx: The number of seconds advertisements are valid
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
 * gssdp_resource_group_get_max_age
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
 * gssdp_resource_group_set_available
 * @resource_group: A #GSSDPResourceGroup
 * @available: TRUE if @resource_group should be available (advertised)
 *
 * Sets @resource_group<!-- -->s availability to @available.
 **/
void
gssdp_resource_group_set_available (GSSDPResourceGroup *resource_group,
                                    gboolean            available)
{
        GList *l;

        g_return_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group));

        if (resource_group->priv->available == available)
                return;

        resource_group->priv->available = available;

        if (available) {
                /* Add re-announcement timer */
                resource_group->priv->timeout_id =
                        g_timeout_add (resource_group->priv->max_age * 1000,
                                       resource_group_timeout,
                                       resource_group);

                /* Announce all resources */
                for (l = resource_group->priv->resources; l; l = l->next)
                        resource_alive (l->data);
        } else {
                /* Unannounce all resources */
                for (l = resource_group->priv->resources; l; l = l->next)
                        resource_byebye (l->data);

                /* Remove re-announcement timer */
                g_source_remove (resource_group->priv->timeout_id);
                resource_group->priv->timeout_id = 0;
        }
        
        g_object_notify (G_OBJECT (resource_group), "available");
}

/**
 * gssdp_resource_group_get_available
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
 * gssdp_resource_group_add_resource
 * @resource_group: An @GSSDPResourceGroup
 * @target: The resource's target
 * @usn: The resource's USN
 * @locations: A #GList of the resource's locations
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

        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), 0);
        g_return_val_if_fail (target != NULL, 0);
        g_return_val_if_fail (usn != NULL, 0);
        g_return_val_if_fail (locations != NULL, 0);

        resource = g_slice_new0 (Resource);

        resource->resource_group = resource_group;

        resource->target = g_strdup (target);
        resource->usn    = g_strdup (usn);

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
 * gssdp_resource_group_add_resource_simple
 * @resource_group: An @GSSDPResourceGroup
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
        Resource *resource;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_GROUP (resource_group), 0);
        g_return_val_if_fail (target != NULL, 0);
        g_return_val_if_fail (usn != NULL, 0);
        g_return_val_if_fail (location != NULL, 0);

        resource = g_slice_new0 (Resource);

        resource->resource_group = resource_group;

        resource->target = g_strdup (target);
        resource->usn    = g_strdup (usn);

        resource->locations = g_list_append (resource->locations,
                                             g_strdup (location));

        resource_group->priv->resources =
                g_list_prepend (resource_group->priv->resources, resource);

        resource->id = ++resource_group->priv->last_resource_id;

        if (resource_group->priv->available)
                resource_alive (resource);

        return resource->id;
}

/**
 * gssdp_resource_group_remove_resource
 * @resource_group: An @GSSDPResourceGroup
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

/**
 * Called every max-age seconds to re-announce all resources
 **/
static gboolean
resource_group_timeout (gpointer user_data)
{
        GSSDPResourceGroup *resource_group;
        GList *l;

        resource_group = GSSDP_RESOURCE_GROUP (user_data);

        /* Re-announce all resources */
        for (l = resource_group->priv->resources; l; l = l->next)
                resource_alive (l->data);

        return TRUE;
}

/**
 * Received a message
 **/
static void
message_received_cb (GSSDPClient      *client,
                     const char       *from_ip,
                     _GSSDPMessageType type,
                     GHashTable       *headers,
                     gpointer          user_data)
{
        GSSDPResourceGroup *resource_group;
        GSList *list;
        const char *target;
        gboolean want_all;
        int mx;
        GList *l;

        resource_group = GSSDP_RESOURCE_GROUP (user_data);

        /* Only process if we are available */
        if (!resource_group->priv->available)
                return;

        /* We only handle discovery requests */
        if (type != _GSSDP_DISCOVERY_REQUEST)
                return;

        /* Extract target */
        list = g_hash_table_lookup (headers, "ST");
        if (!list) {
                g_warning ("Discovery request did not have an ST header");

                return;
        }

        target = list->data;

        /* Is this the "ssdp:all" target? */
        want_all = (strcmp (target, SSDP_ALL_RESOURCES) == 0);

        /* Extract MX */
        list = g_hash_table_lookup (headers, "MX");
        if (list)
                mx = atoi (list->data);
        else
                mx = SSDP_DEFAULT_MX;

        /* Find matching resource */
        for (l = resource_group->priv->resources; l; l = l->next) {
                Resource *resource;

                resource = l->data;

                if (want_all || strcmp (resource->target, target) == 0) {
                        /* Match. */
                        guint timeout;
                        DiscoveryResponse *response;

                        /* Get a random timeout from the interval [0, mx] */
                        timeout = g_random_int_range (0, mx * 1000);

                        /* Prepare response */
                        response = g_slice_new (DiscoveryResponse);
                        
                        response->dest_ip = g_strdup (from_ip);
                        response->resource = resource;

                        /* Add timeout */
                        response->timeout_id =
                                g_timeout_add (timeout,
                                               discovery_response_timeout,
                                               response);
                        
                        /* Add to resource */
                        resource->responses =
                                g_list_prepend (resource->responses, response);
                }
        }
}

/**
 * Construct the AL (Alternative Locations) header for @resource
 **/
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

/**
 * Send a discovery response
 **/
static gboolean
discovery_response_timeout (gpointer user_data)
{
        DiscoveryResponse *response;
        GSSDPClient *client;
        char *al, *message;
        guint max_age;

        response = user_data;

        /* Send message */
        client = response->resource->resource_group->priv->client;

        max_age = response->resource->resource_group->priv->max_age;

        al = construct_al (response->resource);

        message = g_strdup_printf (SSDP_DISCOVERY_RESPONSE,
                                   (char *) response->resource->locations->data,
                                   al ? al : "",
                                   response->resource->usn,
                                   gssdp_client_get_server_id (client),
                                   max_age,
                                   response->resource->target);

        _gssdp_client_send_message (client,
                                    response->dest_ip,
                                    message);

        g_free (message);
        g_free (al);

        discovery_response_free (response);

        return FALSE;
}

/**
 * Free a DiscoveryResponse structure and its contained data
 **/
static void
discovery_response_free (DiscoveryResponse *response)
{
        response->resource->responses =
                g_list_remove (response->resource->responses, response);

        g_source_remove (response->timeout_id);
        
        g_free (response->dest_ip);

        g_slice_free (DiscoveryResponse, response);
}

/**
 * Send ssdp:alive message for @resource
 **/
static void
resource_alive (Resource *resource)
{
        GSSDPClient *client;
        guint max_age;
        char *al, *message;

        /* Send message */
        client = resource->resource_group->priv->client;

        max_age = resource->resource_group->priv->max_age;

        al = construct_al (resource);

        message = g_strdup_printf (SSDP_ALIVE_MESSAGE,
                                   max_age,
                                   (char *) resource->locations->data,
                                   al ? al : "",
                                   gssdp_client_get_server_id (client),
                                   resource->target,
                                   resource->usn);

        _gssdp_client_send_message (client,
                                    NULL,
                                    message);

        g_free (message);
        g_free (al);
}

/**
 * Send ssdp:byebye message for @resource
 **/
static void
resource_byebye (Resource *resource)
{
        GSSDPClient *client;
        char *message;

        /* Send message */
        client = resource->resource_group->priv->client;

        message = g_strdup_printf (SSDP_BYEBYE_MESSAGE,
                                   resource->target,
                                   resource->usn);
        
        _gssdp_client_send_message (client,
                                    NULL,
                                    message);

        g_free (message);
}

/**
 * Free a Resource structure and its contained data
 **/
static void
resource_free (Resource *resource)
{
        while (resource->responses)
                discovery_response_free (resource->responses->data);

        if (resource->resource_group->priv->available)
                resource_byebye (resource);

        g_free (resource->usn);
        g_free (resource->target);

        while (resource->locations) {
                g_free (resource->locations->data);
                resource->locations = g_list_delete_link (resource->locations,
                                                         resource->locations);
        }

        g_slice_free (Resource, resource);
}
