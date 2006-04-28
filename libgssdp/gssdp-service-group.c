/* 
 * Copyright (C) 2006 OpenedHand Ltd.
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

#include "gssdp-service-group.h"
#include "gssdp-error.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"

G_DEFINE_TYPE (GSSDPServiceGroup,
               gssdp_service_group,
               G_TYPE_OBJECT);

struct _GSSDPServiceGroupPrivate {
        GSSDPClient *client;

        guint        max_age;

        gboolean     available;

        GList       *services;

        gulong       message_received_id;

        guint        last_service_id;
};

enum {
        PROP_0,
        PROP_CLIENT,
        PROP_MAX_AGE,
        PROP_AVAILABLE
};

typedef struct {
        GSSDPServiceGroup *service_group;

        char              *target;
        char              *usn;
        GList             *locations;

        guint              timeout_id;

        GList             *responses;

        guint              id;
} Service;

typedef struct {
        char    *dest_ip;
        Service *service;

        guint    timeout_id;
} DiscoveryResponse;

/* Function prototypes */
static void
gssdp_service_group_set_client (GSSDPServiceGroup *service_group,
                                GSSDPClient       *client);
static void
message_received_cb            (GSSDPClient       *client,
                                const char        *from_ip,
                                _GSSDPMessageType  type,
                                GHashTable        *headers,
                                gpointer           user_data);
static gboolean
service_alive                  (Service           *service);
static void
service_byebye                 (Service           *service);
static void
service_free                   (Service           *service);
static gboolean
discovery_response_timeout     (gpointer           user_data);
static void
discovery_response_free        (DiscoveryResponse *response);

static void
gssdp_service_group_init (GSSDPServiceGroup *service_group)
{
        service_group->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (service_group,
                                         GSSDP_TYPE_SERVICE_GROUP,
                                         GSSDPServiceGroupPrivate);

        service_group->priv->max_age = SSDP_DEFAULT_MAX_AGE;
}

static void
gssdp_service_group_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
        GSSDPServiceGroup *service_group;

        service_group = GSSDP_SERVICE_GROUP (object);

        switch (property_id) {
        case PROP_CLIENT:
                g_value_set_object
                        (value,
                         gssdp_service_group_get_client (service_group));
                break;
        case PROP_MAX_AGE:
                g_value_set_uint
                        (value,
                         gssdp_service_group_get_max_age (service_group));
                break;
        case PROP_AVAILABLE:
                g_value_set_boolean
                        (value,
                         gssdp_service_group_get_available (service_group));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_service_group_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        GSSDPServiceGroup *service_group;

        service_group = GSSDP_SERVICE_GROUP (object);

        switch (property_id) {
        case PROP_CLIENT:
                gssdp_service_group_set_client (service_group,
                                                g_value_get_object (value));
                break;
        case PROP_MAX_AGE:
                gssdp_service_group_set_max_age (service_group,
                                                 g_value_get_long (value));
                break;
        case PROP_AVAILABLE:
                gssdp_service_group_set_available (service_group,
                                                   g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_service_group_dispose (GObject *object)
{
        GSSDPServiceGroup *service_group;

        service_group = GSSDP_SERVICE_GROUP (object);

        if (service_group->priv->client) {
                if (g_signal_handler_is_connected
                        (service_group->priv->client,
                         service_group->priv->message_received_id)) {
                        g_signal_handler_disconnect
                                (service_group->priv->client,
                                 service_group->priv->message_received_id);
                }
                                                   
                g_object_unref (service_group->priv->client);
                service_group->priv->client = NULL;
        }
        
        while (service_group->priv->services) {
                service_free (service_group->priv->services->data);
                service_group->priv->services =
                        g_list_delete_link (service_group->priv->services,
                                            service_group->priv->services);
        }
}

static void
gssdp_service_group_class_init (GSSDPServiceGroupClass *klass)
{
        GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gssdp_service_group_set_property;
	object_class->get_property = gssdp_service_group_get_property;
	object_class->dispose      = gssdp_service_group_dispose;

        g_type_class_add_private (klass, sizeof (GSSDPServiceGroupPrivate));

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
                          "TRUE if the services are available (advertised).",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));
}

/**
 * gssdp_service_group_new
 * @client: The #GSSDPClient to associate with
 * @error: A location to return an error of type #GSSDP_ERROR_QUARK
 *
 * Return value: A new #GSSDPServiceGroup object.
 **/
GSSDPServiceGroup *
gssdp_service_group_new (GSSDPClient *client)
{
        return g_object_new (GSSDP_TYPE_SERVICE_GROUP,
                             "client", client,
                             NULL);
}

/**
 * Sets the #GSSDPClient @service_group is associated with to @client
 **/
static void
gssdp_service_group_set_client (GSSDPServiceGroup *service_group,
                                GSSDPClient         *client)
{
        g_return_if_fail (GSSDP_IS_SERVICE_GROUP (service_group));
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        service_group->priv->client = g_object_ref (client);

        service_group->priv->message_received_id =
                g_signal_connect_object (service_group->priv->client,
                                         "message-received",
                                         G_CALLBACK (message_received_cb),
                                         service_group,
                                         0);

        g_object_notify (G_OBJECT (service_group), "client");
}

/**
 * gssdp_service_group_get_client
 * @service_group: A #GSSDPServiceGroup
 *
 * Return value: The #GSSDPClient @service_group is associated with.
 **/
GSSDPClient *
gssdp_service_group_get_client (GSSDPServiceGroup *service_group)
{
        g_return_val_if_fail (GSSDP_IS_SERVICE_GROUP (service_group), NULL);

        return service_group->priv->client;
}

/**
 * gssdp_service_group_set_max_age
 * @service_group: A #GSSDPServiceGroup
 * @mx: The number of seconds advertisements are valid
 *
 * Sets the number of seconds advertisements are valid to @max_age.
 **/
void
gssdp_service_group_set_max_age (GSSDPServiceGroup *service_group,
                                 guint              max_age)
{
        g_return_if_fail (GSSDP_IS_SERVICE_GROUP (service_group));

        if (service_group->priv->max_age == max_age)
                return;

        service_group->priv->max_age = max_age;
        
        g_object_notify (G_OBJECT (service_group), "max-age");
}

/**
 * gssdp_service_group_get_max_age
 * @service_group: A #GSSDPServiceGroup
 *
 * Return value: The number of seconds advertisements are valid.
 **/
guint
gssdp_service_group_get_max_age (GSSDPServiceGroup *service_group)
{
        g_return_val_if_fail (GSSDP_IS_SERVICE_GROUP (service_group), 0);

        return service_group->priv->max_age;
}

/**
 * gssdp_service_group_set_available
 * @service_group: A #GSSDPServiceGroup
 * @available: TRUE if @service_group should be available (advertised)
 *
 * Sets @service_group<!-- -->s availability to @available.
 **/
void
gssdp_service_group_set_available (GSSDPServiceGroup *service_group,
                                   gboolean           available)
{
        GList *l;

        g_return_if_fail (GSSDP_IS_SERVICE_GROUP (service_group));

        if (service_group->priv->available == available)
                return;

        service_group->priv->available = available;

        if (available) {
                /* Try to announce all services */
                for (l = service_group->priv->services; l; l = l->next)
                        service_alive (l->data);
        } else {
                /* Try to unannounce all services */
                for (l = service_group->priv->services; l; l = l->next)
                        service_byebye (l->data);
        }
        
        g_object_notify (G_OBJECT (service_group), "available");
}

/**
 * gssdp_service_group_get_available
 * @service_group: A #GSSDPServiceGroup
 *
 * Return value: TRUE if @service_group is available (advertised).
 **/
gboolean
gssdp_service_group_get_available (GSSDPServiceGroup *service_group)
{
        g_return_val_if_fail (GSSDP_IS_SERVICE_GROUP (service_group), FALSE);

        return service_group->priv->available;
}

/**
 * gssdp_service_group_add_service
 * @service_group: An @GSSDPServiceGroup
 * @target: The service's target
 * @usn: The service's USN
 * @locations: A #GList of the service's locations
 *
 * Adds a service with target @target, USN @usn, and locations @locations
 * to @service_group.
 *
 * Return value: The ID of the added service.
 **/
guint
gssdp_service_group_add_service (GSSDPServiceGroup *service_group,
                                 const char        *target,
                                 const char        *usn,
                                 GList             *locations)
{
        Service *service;
        GList *l;

        g_return_val_if_fail (GSSDP_IS_SERVICE_GROUP (service_group), 0);
        g_return_val_if_fail (target != NULL, 0);
        g_return_val_if_fail (usn != NULL, 0);
        g_return_val_if_fail (locations != NULL, 0);

        service = g_slice_new0 (Service);

        service->service_group = service_group;

        service->target = g_strdup (target);
        service->usn    = g_strdup (usn);

        for (l = locations; l; l = l->next) {
                service->locations = g_list_append (service->locations,
                                                    g_strdup (l->data));
        }

        service_group->priv->services =
                g_list_prepend (service_group->priv->services, service);

        service->id = ++service_group->priv->last_service_id;

        if (service_group->priv->available)
                service_alive (service);

        return service->id;
}

/**
 * gssdp_service_group_add_service_simple
 * @service_group: An @GSSDPServiceGroup
 * @target: The service's target
 * @usn: The service's USN
 * @location: The service's location
 *
 * Adds a service with target @target, USN @usn, and location @location
 * to @service_group.
 *
 * Return value: The ID of the added service.
 **/
guint
gssdp_service_group_add_service_simple (GSSDPServiceGroup *service_group,
                                        const char        *target,
                                        const char        *usn,
                                        const char        *location)
{
        Service *service;

        g_return_val_if_fail (GSSDP_IS_SERVICE_GROUP (service_group), 0);
        g_return_val_if_fail (target != NULL, 0);
        g_return_val_if_fail (usn != NULL, 0);
        g_return_val_if_fail (location != NULL, 0);

        service = g_slice_new0 (Service);

        service->service_group = service_group;

        service->target = g_strdup (target);
        service->usn    = g_strdup (usn);

        service->locations = g_list_append (service->locations,
                                            g_strdup (location));

        service_group->priv->services =
                g_list_prepend (service_group->priv->services, service);

        service->id = ++service_group->priv->last_service_id;

        if (service_group->priv->available)
                service_alive (service);

        return service->id;
}

/**
 * gssdp_service_group_remove_service
 * @service_group: An @GSSDPServiceGroup
 * @service_id: The ID of the service to remove
 *
 * Removes the service with ID @service_id from @service_group.
 **/
void
gssdp_service_group_remove_service (GSSDPServiceGroup *service_group,
                                    guint              service_id)
{
        GList *l;

        g_return_if_fail (GSSDP_IS_SERVICE_GROUP (service_group));
        g_return_if_fail (service_id > 0);

        for (l = service_group->priv->services; l; l = l->next) {
                Service *service;

                service = l->data;

                if (service->id == service_id) {
                        service_group->priv->services = 
                                g_list_remove (service_group->priv->services,
                                               service);
                        
                        service_free (service);

                        return;
                }
        }
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
        GSSDPServiceGroup *service_group;
        GSList *list;
        const char *target;
        GList *l;

        service_group = GSSDP_SERVICE_GROUP (user_data);

        /* Only process if we are available */
        if (!service_group->priv->available)
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

        /* Find matching service */
        for (l = service_group->priv->services; l; l = l->next) {
                Service *service;

                service = l->data;

                if (strcmp (service->target, target) == 0) {
                        /* Match. Extract MX */
                        int mx;
                        guint timeout;
                        DiscoveryResponse *response;

                        list = g_hash_table_lookup (headers, "MX");
                        if (list)
                                mx = atoi (list->data);
                        else
                                mx = SSDP_DEFAULT_MX;

                        /* Get a random timeout within the [0..mx] interval */
                        timeout = g_random_int_range (0, mx * 1000);

                        /* Prepare response */
                        response = g_slice_new (DiscoveryResponse);
                        
                        response->dest_ip = g_strdup (from_ip);
                        response->service = service;

                        /* Add timeout */
                        response->timeout_id =
                                g_timeout_add (timeout,
                                               discovery_response_timeout,
                                               response);
                        
                        /* Add to service */
                        service->responses =
                                g_list_prepend (service->responses, response);

                        return;
                }
        }
}

/**
 * Construct the AL header for @service
 **/
static char *
construct_al (Service *service)
{
       if (service->locations->next) {
                GString *al_string;
                GList *l;

                al_string = g_string_new ("AL: ");

                for (l = service->locations->next; l; l = l->next) {
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
        client = response->service->service_group->priv->client;

        max_age = response->service->service_group->priv->max_age;

        al = construct_al (response->service);

        message = g_strdup_printf (SSDP_DISCOVERY_RESPONSE,
                                   (char *) response->service->locations->data,
                                   al ? al : "",
                                   response->service->usn,
                                   gssdp_client_get_server_id (client),
                                   max_age,
                                   response->service->target);

        _gssdp_client_send_message (client,
                                    response->dest_ip,
                                    message,
                                    NULL);

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
        response->service->responses =
                g_list_remove (response->service->responses, response);

        g_source_remove (response->timeout_id);
        
        g_free (response->dest_ip);

        g_slice_free (DiscoveryResponse, response);
}

/**
 * Send ssdp:alive message for @service
 **/
static gboolean
service_alive (Service *service)
{
        GSSDPClient *client;
        guint max_age;
        char *al, *message;

        max_age = service->service_group->priv->max_age;

        /* Add timeout */
        if (service->timeout_id == 0) {
                guint timeout;

                timeout = max_age * 1000;

                service->timeout_id =
                        g_timeout_add (timeout,
                                       (GSourceFunc) service_alive,
                                       service);
        }

        /* Send message */
        client = service->service_group->priv->client;

        al = construct_al (service);

        message = g_strdup_printf (SSDP_ALIVE_MESSAGE,
                                   max_age,
                                   (char *) service->locations->data,
                                   al ? al : "",
                                   gssdp_client_get_server_id (client),
                                   service->target,
                                   service->usn);

        _gssdp_client_send_message (client,
                                    NULL,
                                    message,
                                    NULL);

        g_free (message);
        g_free (al);

        return TRUE;
}

/**
 * Send ssdp:byebye message for @service
 **/
static void
service_byebye (Service *service)
{
        GSSDPClient *client;
        char *message;

        if (service->timeout_id == 0)
                return; /* Aleady dead */
        
        /* Send message */
        client = service->service_group->priv->client;

        message = g_strdup_printf (SSDP_BYEBYE_MESSAGE,
                                   service->target,
                                   service->usn);
        
        _gssdp_client_send_message (client,
                                    NULL,
                                    message,
                                    NULL);

        g_free (message);

        /* Remove timeout */
        g_source_remove (service->timeout_id);
        service->timeout_id = 0;
}

/**
 * Free a Service structure and its contained data
 **/
static void
service_free (Service *service)
{
        while (service->responses)
                discovery_response_free (service->responses->data);

        if (service->service_group->priv->available)
                service_byebye (service);

        g_free (service->usn);
        g_free (service->target);

        while (service->locations) {
                g_free (service->locations->data);
                service->locations = g_list_delete_link (service->locations,
                                                         service->locations);
        }

        g_slice_free (Service, service);
}
