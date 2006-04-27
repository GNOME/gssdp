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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "gssdp-service-browser.h"
#include "gssdp-error.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"
#include "gssdp-marshal.h"

G_DEFINE_TYPE (GSSDPServiceBrowser,
               gssdp_service_browser,
               G_TYPE_OBJECT);

struct _GSSDPServiceBrowserPrivate {
        GSSDPClient *client;

        char        *target;

        gushort      mx;

        gboolean     active;

        gulong       message_received_id;

        GHashTable  *services;
};

enum {
        PROP_0,
        PROP_CLIENT,
        PROP_TARGET,
        PROP_MX,
        PROP_ACTIVE
};

enum {
        SERVICE_AVAILABLE,
        SERVICE_UNAVAILABLE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
        GSSDPServiceBrowser *service_browser;
        char                *usn;
        guint                timeout_id;
} Service;

/* Function prototypes */
static void
gssdp_service_browser_set_client (GSSDPServiceBrowser *service_browser,
                                  GSSDPClient         *client);
static void
message_received_cb              (GSSDPClient         *client,
                                  const char          *from_ip,
                                  _GSSDPMessageType    type,
                                  GHashTable          *headers,
                                  gpointer             user_data);
static void
service_free                     (gpointer             data);
static void
clear_cache                      (GSSDPServiceBrowser *service_browser);

static void
gssdp_service_browser_init (GSSDPServiceBrowser *service_browser)
{
        service_browser->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (service_browser,
                                         GSSDP_TYPE_SERVICE_BROWSER,
                                         GSSDPServiceBrowserPrivate);

        service_browser->priv->mx = SSDP_DEFAULT_MX;

        service_browser->priv->services = g_hash_table_new_full (g_str_hash,
                                                                 g_str_equal,
                                                                 NULL,
                                                                 service_free);
}

static void
gssdp_service_browser_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        GSSDPServiceBrowser *service_browser;

        service_browser = GSSDP_SERVICE_BROWSER (object);

        switch (property_id) {
        case PROP_CLIENT:
                g_value_set_object
                        (value,
                         gssdp_service_browser_get_client (service_browser));
                break;
        case PROP_TARGET:
                g_value_set_string
                        (value,
                         gssdp_service_browser_get_target (service_browser));
                break;
        case PROP_MX:
                g_value_set_uint
                        (value,
                         gssdp_service_browser_get_mx (service_browser));
                break;
        case PROP_ACTIVE:
                g_value_set_boolean
                        (value,
                         gssdp_service_browser_get_active (service_browser));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_service_browser_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        GSSDPServiceBrowser *service_browser;

        service_browser = GSSDP_SERVICE_BROWSER (object);

        switch (property_id) {
        case PROP_CLIENT:
                gssdp_service_browser_set_client (service_browser,
                                                  g_value_get_object (value));
                break;
        case PROP_TARGET:
                gssdp_service_browser_set_target (service_browser,
                                                  g_value_get_string (value));
                break;
        case PROP_MX:
                gssdp_service_browser_set_mx (service_browser,
                                              g_value_get_uint (value));
                break;
        case PROP_ACTIVE:
                gssdp_service_browser_set_active (service_browser,
                                                  g_value_get_boolean (value),
                                                  NULL);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_service_browser_dispose (GObject *object)
{
        GSSDPServiceBrowser *service_browser;

        service_browser = GSSDP_SERVICE_BROWSER (object);

        if (service_browser->priv->client) {
                if (g_signal_handler_is_connected
                        (service_browser->priv->client,
                         service_browser->priv->message_received_id)) {
                        g_signal_handler_disconnect
                                (service_browser->priv->client,
                                 service_browser->priv->message_received_id);
                }
                                                   
                g_object_unref (service_browser->priv->client);
                service_browser->priv->client = NULL;
        }

        clear_cache (service_browser);
}

static void
gssdp_service_browser_finalize (GObject *object)
{
        GSSDPServiceBrowser *service_browser;

        service_browser = GSSDP_SERVICE_BROWSER (object);

        g_free (service_browser->priv->target);

        g_hash_table_destroy (service_browser->priv->services);
}

static void
gssdp_service_browser_class_init (GSSDPServiceBrowserClass *klass)
{
        GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gssdp_service_browser_set_property;
	object_class->get_property = gssdp_service_browser_get_property;
	object_class->dispose      = gssdp_service_browser_dispose;
	object_class->finalize     = gssdp_service_browser_finalize;

        g_type_class_add_private (klass, sizeof (GSSDPServiceBrowserPrivate));

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
                 PROP_TARGET,
                 g_param_spec_string
                         ("target",
                          "Target",
                          "The browser target.",
                          NULL,
                          G_PARAM_READWRITE |
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
                          SSDP_DEFAULT_MX,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_ACTIVE,
                 g_param_spec_boolean
                         ("active",
                          "Active",
                          "TRUE if the service browser is active.",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        signals[SERVICE_AVAILABLE] =
                g_signal_new ("service-available",
                              GSSDP_TYPE_SERVICE_BROWSER,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GSSDPServiceBrowserClass,
                                               service_available),
                              NULL, NULL,
                              gssdp_marshal_VOID__STRING_POINTER,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
                              G_TYPE_POINTER);

        signals[SERVICE_UNAVAILABLE] =
                g_signal_new ("service-unavailable",
                              GSSDP_TYPE_SERVICE_BROWSER,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GSSDPServiceBrowserClass,
                                               service_unavailable),
                              NULL, NULL,
                              gssdp_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_STRING);
}

/**
 * gssdp_service_browser_new
 * @client: The #GSSDPClient to associate with
 * @error: A location to return an error of type #GSSDP_ERROR_QUARK
 *
 * Return value: A new #GSSDPServiceBrowser object.
 **/
GSSDPServiceBrowser *
gssdp_service_browser_new (GSSDPClient *client,
                           const char  *target)
{
        return g_object_new (GSSDP_TYPE_SERVICE_BROWSER,
                             "client", client,
                             "target", target,
                             NULL);
}

/**
 * Sets the #GSSDPClient @service_browser is associated with to @client
 **/
static void
gssdp_service_browser_set_client (GSSDPServiceBrowser *service_browser,
                                  GSSDPClient         *client)
{
        g_return_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser));
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        service_browser->priv->client = g_object_ref (client);

        service_browser->priv->message_received_id =
                g_signal_connect_object (service_browser->priv->client,
                                         "message-received",
                                         G_CALLBACK (message_received_cb),
                                         service_browser,
                                         0);

        g_object_notify (G_OBJECT (service_browser), "client");
}

/**
 * gssdp_service_browser_get_client
 * @service_browser: A #GSSDPServiceBrowser
 *
 * Return value: The #GSSDPClient @service_browser is associated with.
 **/
GSSDPClient *
gssdp_service_browser_get_client (GSSDPServiceBrowser *service_browser)
{
        g_return_val_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser), NULL);

        return service_browser->priv->client;
}

/**
 * gssdp_service_browser_set_target
 * @service_browser: A #GSSDPServiceBrowser
 * @target: The browser target
 *
 * Sets the browser target of @service_browser to @target.
 **/
void
gssdp_service_browser_set_target (GSSDPServiceBrowser *service_browser,
                                  const char          *target)
{
        g_return_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser));
        g_return_if_fail (target != NULL);
        g_return_if_fail (!service_browser->priv->active);
        
        g_free (service_browser->priv->target);
        service_browser->priv->target = g_strdup (target);

        g_object_notify (G_OBJECT (service_browser), "target");
}

/**
 * gssdp_service_browser_get_target
 * @service_browser: A #GSSDPServiceBrowser
 *
 * Return value: The browser target.
 **/
const char *
gssdp_service_browser_get_target (GSSDPServiceBrowser *service_browser)
{
        g_return_val_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser), NULL);

        return service_browser->priv->target;
}

/**
 * gssdp_service_browser_set_mx
 * @service_browser: A #GSSDPServiceBrowser
 * @mx: The to be used MX value
 *
 * Sets the used MX value of @service_browser to @mx.
 **/
void
gssdp_service_browser_set_mx (GSSDPServiceBrowser *service_browser,
                              gushort              mx)
{
        g_return_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser));

        if (service_browser->priv->mx == mx)
                return;

        service_browser->priv->mx = mx;
        
        g_object_notify (G_OBJECT (service_browser), "mx");
}

/**
 * gssdp_service_browser_get_mx
 * @service_browser: A #GSSDPServiceBrowser
 *
 * Return value: The used MX value.
 **/
gushort
gssdp_service_browser_get_mx (GSSDPServiceBrowser *service_browser)
{
        g_return_val_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser), 0);

        return service_browser->priv->mx;
}

/**
 * gssdp_service_browser_set_active
 * @service_browser: A #GSSDPServiceBrowser
 * @active: TRUE to activate @service_browser
 * @error: A location to return an error of type #GSSDP_ERROR_QUARK
 *
 * (De)activates @service_browser.
 *
 * Return value: TRUE if the (de)activation succeeded.
 **/
gboolean
gssdp_service_browser_set_active (GSSDPServiceBrowser *service_browser,
                                  gboolean             active,
                                  GError             **error)
{
        g_return_val_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser),
                              FALSE);

        if (service_browser->priv->active == active)
                return TRUE;

        if (active) {
                /* Emit discovery message */
                char *message;
                gboolean res;

                message = g_strdup_printf (SSDP_DISCOVERY_REQUEST,
                                           service_browser->priv->target,
                                           service_browser->priv->mx);

                res = _gssdp_client_send_message (service_browser->priv->client,
                                                  NULL,
                                                  message,
                                                  error);

                g_free (message);

                if (!res)
                        return FALSE;
        } else
                clear_cache (service_browser);

        service_browser->priv->active = active;
        
        g_object_notify (G_OBJECT (service_browser), "active");

        return TRUE;
}

/**
 * gssdp_service_browser_get_active
 * @service_browser: A #GSSDPServiceBrowser
 *
 * Return value: TRUE if @service_browser is active.
 **/
gboolean
gssdp_service_browser_get_active (GSSDPServiceBrowser *service_browser)
{
        g_return_val_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser), 0);

        return service_browser->priv->active;
}

/**
 * Service expired: Remove
 **/
static gboolean
service_expire (gpointer user_data)
{
        Service *service;

        service = user_data;
        
        g_signal_emit (service->service_browser,
                       signals[SERVICE_UNAVAILABLE],
                       0,
                       service->usn);

        g_hash_table_remove (service->service_browser->priv->services,
                             service->usn);

        return FALSE;
}

static void
service_available (GSSDPServiceBrowser *service_browser,
                   GHashTable          *headers)
{
        GSList *list;
        const char *usn;
        Service *service;
        gboolean was_cached;
        guint timeout;
        GList *locations;

        list = g_hash_table_lookup (headers, "USN");
        if (!list)
                return; /* No USN specified */
        usn = list->data;

        /* Get from cache, if possible */
        service = g_hash_table_lookup (service_browser->priv->services, usn);
        if (service) {
                /* Remove old timeout */
                g_source_remove (service->timeout_id);

                was_cached = TRUE;
        } else {
                /* Create new Service data structure */
                service = g_slice_new (Service);

                service->service_browser = service_browser;
                service->usn             = g_strdup (usn);
                
                g_hash_table_insert (service_browser->priv->services,
                                     service->usn,
                                     service);
                
                was_cached = FALSE;
        }

        /* Calculate new timeout */
        list = g_hash_table_lookup (headers, "Cache-Control");
        if (list) {
                if (sscanf (list->data,
                            "max-age=%d",
                            &timeout) < 1) {
                        g_warning ("Invalid 'Cache-Control' header. Assuming "
                                   "default max-age of %d.\n"
                                   "Header was:\n%s",
                                   SSDP_MIN_MAX_AGE,
                                   (char *) list->data);

                        timeout = SSDP_MIN_MAX_AGE;
                }
        } else {
                list = g_hash_table_lookup (headers, "Expires");
                if (list) {
                        struct tm expiration_date;
                        time_t exp_time, cur_time;

                        /* GNU strptime parses both local and international
                         * weekdays and months */
                        strptime (list->data,
                                  "%a, %d %b %Y %T %z",
                                  &expiration_date);

                        exp_time = mktime (&expiration_date);
                        cur_time = time (NULL);

                        if (exp_time > cur_time)
                                timeout = exp_time - cur_time;
                        else {
                                g_warning ("Invalid 'Expires' header. Assuming "
                                           "default max-age of %d.\n"
                                           "Header was:\n%s",
                                           SSDP_MIN_MAX_AGE,
                                           (char *) list->data);

                                timeout = SSDP_MIN_MAX_AGE;
                        }
                } else {
                        g_warning ("No 'Cache-Control' nor any 'Expires' "
                                   "header was specified. Assuming default "
                                   "max-age of %d.", SSDP_MIN_MAX_AGE);

                        timeout = SSDP_MIN_MAX_AGE;
                }
        }

        service->timeout_id = g_timeout_add (timeout * 1000,
                                             service_expire,
                                             service);

        /* Only continue with signal emission if this service was not
         * cached already */
        if (was_cached)
                return;

        /* Build list of locations */
        locations = NULL;

        list = g_hash_table_lookup (headers, "Location");
        if (list)
                locations = g_list_append (locations, g_strdup (list->data));

        list = g_hash_table_lookup (headers, "AL");
        if (list) {
                /* Parse AL header. The format is:
                 * <uri1><uri2>... */
                char *start, *end, *uri;
                
                start = list->data;
                while ((start = strchr (start, '<'))) {
                        start += 1;
                        if (!start || !*start)
                                break;

                        end = strchr (start, '>');
                        if (!end || !*end)
                                break;

                        uri = g_strndup (start, end - start);
                        locations = g_list_append (locations, uri);

                        start = end;
                }
        }

        /* Emit signal */
        g_signal_emit (service_browser,
                       signals[SERVICE_AVAILABLE],
                       0,
                       usn,
                       locations);

        /* Cleanup */
        while (locations) {
                g_free (locations->data);

                locations = g_list_delete_link (locations, locations);
        }
}

static void
service_unavailable (GSSDPServiceBrowser *service_browser,
                     GHashTable          *headers)
{
        GSList *list;
        const char *usn;

        list = g_hash_table_lookup (headers, "USN");
        if (!list)
                return; /* No USN specified */
        usn = list->data;

        /* Only process if we were cached */
        if (!g_hash_table_lookup (service_browser->priv->services, usn))
                return;

        g_signal_emit (service_browser,
                       signals[SERVICE_UNAVAILABLE],
                       0,
                       usn);

        g_hash_table_remove (service_browser->priv->services, usn);
}

static void
received_discovery_response (GSSDPServiceBrowser *service_browser,
                             GHashTable          *headers)
{
        GSList *list;

        list = g_hash_table_lookup (headers, "ST");
        if (!list)
                return; /* No target specified */

        if (strcmp (service_browser->priv->target, list->data) != 0)
                return; /* Target doesn't match */

        service_available (service_browser, headers);
}

static void
received_announcement (GSSDPServiceBrowser *service_browser,
                       GHashTable          *headers)
{
        GSList *list;

        list = g_hash_table_lookup (headers, "NT");
        if (!list)
                return; /* No target specified */

        if (strcmp (service_browser->priv->target, list->data) != 0)
                return; /* Target doesn't match */

        list = g_hash_table_lookup (headers, "NTS");
        if (!list)
                return; /* No announcement type specified */

        /* Check announcement type */
        if      (strncmp (list->data,
                          SSDP_ALIVE_NTS,
                          strlen (SSDP_ALIVE_NTS)) == 0)
                service_available (service_browser, headers);
        else if (strncmp (list->data,
                          SSDP_BYEBYE_NTS,
                          strlen (SSDP_BYEBYE_NTS)) == 0)
                service_unavailable (service_browser, headers);
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
        GSSDPServiceBrowser *service_browser;

        service_browser = GSSDP_SERVICE_BROWSER (user_data);

        if (!service_browser->priv->active)
                return;

        switch (type) {
        case _GSSDP_DISCOVERY_RESPONSE:
                received_discovery_response (service_browser, headers);
                break;
        case _GSSDP_ANNOUNCEMENT:
                received_announcement (service_browser, headers);
                break;
        default:
                break;
        }
}

/**
 * Free a Service structure and its contained data
 **/
static void
service_free (gpointer data)
{
        Service *service;

        service = data;

        g_free (service->usn);

        g_source_remove (service->timeout_id);

        g_slice_free (Service, service);
}

static gboolean
clear_cache_helper (gpointer key, gpointer value, gpointer data)
{
        Service *service;

        service = value;

        g_signal_emit (service->service_browser,
                       signals[SERVICE_UNAVAILABLE],
                       0,
                       service->usn);

        return TRUE;
}

/**
 * Clears the cached services hash
 **/
static void
clear_cache (GSSDPServiceBrowser *service_browser)
{
        /* Clear cache */
        g_hash_table_foreach_remove (service_browser->priv->services,
                                     clear_cache_helper,
                                     NULL);
}
