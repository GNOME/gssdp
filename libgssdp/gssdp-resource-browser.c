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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "gssdp-resource-browser.h"
#include "gssdp-error.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"
#include "gssdp-marshal.h"

G_DEFINE_TYPE (GSSDPResourceBrowser,
               gssdp_resource_browser,
               G_TYPE_OBJECT);

struct _GSSDPResourceBrowserPrivate {
        GSSDPClient *client;

        char        *target;

        gushort      mx;

        gboolean     active;

        gulong       message_received_id;

        GHashTable  *resources;
};

enum {
        PROP_0,
        PROP_CLIENT,
        PROP_TARGET,
        PROP_MX,
        PROP_ACTIVE
};

enum {
        RESOURCE_AVAILABLE,
        RESOURCE_UNAVAILABLE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
        GSSDPResourceBrowser *resource_browser;
        char                 *usn;
        guint                 timeout_id;
} Resource;

/* Function prototypes */
static void
gssdp_resource_browser_set_client (GSSDPResourceBrowser *resource_browser,
                                   GSSDPClient          *client);
static void
message_received_cb              (GSSDPClient          *client,
                                  const char           *from_ip,
                                  _GSSDPMessageType     type,
                                  GHashTable           *headers,
                                  gpointer              user_data);
static void
resource_free                    (gpointer              data);
static void
clear_cache                      (GSSDPResourceBrowser *resource_browser);

static void
gssdp_resource_browser_init (GSSDPResourceBrowser *resource_browser)
{
        resource_browser->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (resource_browser,
                                         GSSDP_TYPE_RESOURCE_BROWSER,
                                         GSSDPResourceBrowserPrivate);

        resource_browser->priv->mx = SSDP_DEFAULT_MX;

        resource_browser->priv->resources =
                g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       NULL,
                                       resource_free);
}

static void
gssdp_resource_browser_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
        GSSDPResourceBrowser *resource_browser;

        resource_browser = GSSDP_RESOURCE_BROWSER (object);

        switch (property_id) {
        case PROP_CLIENT:
                g_value_set_object
                        (value,
                         gssdp_resource_browser_get_client (resource_browser));
                break;
        case PROP_TARGET:
                g_value_set_string
                        (value,
                         gssdp_resource_browser_get_target (resource_browser));
                break;
        case PROP_MX:
                g_value_set_uint
                        (value,
                         gssdp_resource_browser_get_mx (resource_browser));
                break;
        case PROP_ACTIVE:
                g_value_set_boolean
                        (value,
                         gssdp_resource_browser_get_active (resource_browser));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_resource_browser_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
        GSSDPResourceBrowser *resource_browser;

        resource_browser = GSSDP_RESOURCE_BROWSER (object);

        switch (property_id) {
        case PROP_CLIENT:
                gssdp_resource_browser_set_client (resource_browser,
                                                   g_value_get_object (value));
                break;
        case PROP_TARGET:
                gssdp_resource_browser_set_target (resource_browser,
                                                   g_value_get_string (value));
                break;
        case PROP_MX:
                gssdp_resource_browser_set_mx (resource_browser,
                                               g_value_get_uint (value));
                break;
        case PROP_ACTIVE:
                gssdp_resource_browser_set_active (resource_browser,
                                                   g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_resource_browser_dispose (GObject *object)
{
        GSSDPResourceBrowser *resource_browser;

        resource_browser = GSSDP_RESOURCE_BROWSER (object);

        if (resource_browser->priv->client) {
                if (g_signal_handler_is_connected
                        (resource_browser->priv->client,
                         resource_browser->priv->message_received_id)) {
                        g_signal_handler_disconnect
                                (resource_browser->priv->client,
                                 resource_browser->priv->message_received_id);
                }
                                                   
                g_object_unref (resource_browser->priv->client);
                resource_browser->priv->client = NULL;
        }

        clear_cache (resource_browser);
}

static void
gssdp_resource_browser_finalize (GObject *object)
{
        GSSDPResourceBrowser *resource_browser;

        resource_browser = GSSDP_RESOURCE_BROWSER (object);

        g_free (resource_browser->priv->target);

        g_hash_table_destroy (resource_browser->priv->resources);
}

static void
gssdp_resource_browser_class_init (GSSDPResourceBrowserClass *klass)
{
        GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gssdp_resource_browser_set_property;
	object_class->get_property = gssdp_resource_browser_get_property;
	object_class->dispose      = gssdp_resource_browser_dispose;
	object_class->finalize     = gssdp_resource_browser_finalize;

        g_type_class_add_private (klass, sizeof (GSSDPResourceBrowserPrivate));

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
                          "TRUE if the resource browser is active.",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        signals[RESOURCE_AVAILABLE] =
                g_signal_new ("resource-available",
                              GSSDP_TYPE_RESOURCE_BROWSER,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GSSDPResourceBrowserClass,
                                               resource_available),
                              NULL, NULL,
                              gssdp_marshal_VOID__STRING_POINTER,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
                              G_TYPE_POINTER);

        signals[RESOURCE_UNAVAILABLE] =
                g_signal_new ("resource-unavailable",
                              GSSDP_TYPE_RESOURCE_BROWSER,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GSSDPResourceBrowserClass,
                                               resource_unavailable),
                              NULL, NULL,
                              gssdp_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_STRING);
}

/**
 * gssdp_resource_browser_new
 * @client: The #GSSDPClient to associate with
 * @error: A location to return an error of type #GSSDP_ERROR_QUARK
 *
 * Return value: A new #GSSDPResourceBrowser object.
 **/
GSSDPResourceBrowser *
gssdp_resource_browser_new (GSSDPClient *client,
                            const char  *target)
{
        return g_object_new (GSSDP_TYPE_RESOURCE_BROWSER,
                             "client", client,
                             "target", target,
                             NULL);
}

/**
 * Sets the #GSSDPClient @resource_browser is associated with to @client
 **/
static void
gssdp_resource_browser_set_client (GSSDPResourceBrowser *resource_browser,
                                   GSSDPClient          *client)
{
        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        resource_browser->priv->client = g_object_ref (client);

        resource_browser->priv->message_received_id =
                g_signal_connect_object (resource_browser->priv->client,
                                         "message-received",
                                         G_CALLBACK (message_received_cb),
                                         resource_browser,
                                         0);

        g_object_notify (G_OBJECT (resource_browser), "client");
}

/**
 * gssdp_resource_browser_get_client
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Return value: The #GSSDPClient @resource_browser is associated with.
 **/
GSSDPClient *
gssdp_resource_browser_get_client (GSSDPResourceBrowser *resource_browser)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser),
                              NULL);

        return resource_browser->priv->client;
}

/**
 * gssdp_resource_browser_set_target
 * @resource_browser: A #GSSDPResourceBrowser
 * @target: The browser target
 *
 * Sets the browser target of @resource_browser to @target.
 **/
void
gssdp_resource_browser_set_target (GSSDPResourceBrowser *resource_browser,
                                   const char           *target)
{
        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));
        g_return_if_fail (target != NULL);
        g_return_if_fail (!resource_browser->priv->active);
        
        g_free (resource_browser->priv->target);
        resource_browser->priv->target = g_strdup (target);

        g_object_notify (G_OBJECT (resource_browser), "target");
}

/**
 * gssdp_resource_browser_get_target
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Return value: The browser target.
 **/
const char *
gssdp_resource_browser_get_target (GSSDPResourceBrowser *resource_browser)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser),
                              NULL);

        return resource_browser->priv->target;
}

/**
 * gssdp_resource_browser_set_mx
 * @resource_browser: A #GSSDPResourceBrowser
 * @mx: The to be used MX value
 *
 * Sets the used MX value of @resource_browser to @mx.
 **/
void
gssdp_resource_browser_set_mx (GSSDPResourceBrowser *resource_browser,
                               gushort               mx)
{
        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));

        if (resource_browser->priv->mx == mx)
                return;

        resource_browser->priv->mx = mx;
        
        g_object_notify (G_OBJECT (resource_browser), "mx");
}

/**
 * gssdp_resource_browser_get_mx
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Return value: The used MX value.
 **/
gushort
gssdp_resource_browser_get_mx (GSSDPResourceBrowser *resource_browser)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser), 0);

        return resource_browser->priv->mx;
}

/**
 * gssdp_resource_browser_set_active
 * @resource_browser: A #GSSDPResourceBrowser
 * @active: TRUE to activate @resource_browser
 *
 * (De)activates @resource_browser.
 **/
void
gssdp_resource_browser_set_active (GSSDPResourceBrowser *resource_browser,
                                   gboolean              active)
{
        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));

        if (resource_browser->priv->active == active)
                return;

        resource_browser->priv->active = active;

        if (active) {
                /* Emit discovery message */
                char *message;

                message = g_strdup_printf (SSDP_DISCOVERY_REQUEST,
                                           resource_browser->priv->target,
                                           resource_browser->priv->mx);

                _gssdp_client_send_message (resource_browser->priv->client,
                                            NULL,
                                            message);

                g_free (message);
        } else
                clear_cache (resource_browser);
        
        g_object_notify (G_OBJECT (resource_browser), "active");
}

/**
 * gssdp_resource_browser_get_active
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Return value: TRUE if @resource_browser is active.
 **/
gboolean
gssdp_resource_browser_get_active (GSSDPResourceBrowser *resource_browser)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser), 0);

        return resource_browser->priv->active;
}

/**
 * Resource expired: Remove
 **/
static gboolean
resource_expire (gpointer user_data)
{
        Resource *resource;

        resource = user_data;
        
        g_signal_emit (resource->resource_browser,
                       signals[RESOURCE_UNAVAILABLE],
                       0,
                       resource->usn);

        g_hash_table_remove (resource->resource_browser->priv->resources,
                             resource->usn);

        return FALSE;
}

static void
resource_available (GSSDPResourceBrowser *resource_browser,
                    GHashTable           *headers)
{
        GSList *list;
        const char *usn;
        Resource *resource;
        gboolean was_cached;
        guint timeout;
        GList *locations;

        list = g_hash_table_lookup (headers, "USN");
        if (!list)
                return; /* No USN specified */
        usn = list->data;

        /* Get from cache, if possible */
        resource = g_hash_table_lookup (resource_browser->priv->resources, usn);
        if (resource) {
                /* Remove old timeout */
                g_source_remove (resource->timeout_id);

                was_cached = TRUE;
        } else {
                /* Create new Resource data structure */
                resource = g_slice_new (Resource);

                resource->resource_browser = resource_browser;
                resource->usn              = g_strdup (usn);
                
                g_hash_table_insert (resource_browser->priv->resources,
                                     resource->usn,
                                     resource);
                
                was_cached = FALSE;
        }

        /* Calculate new timeout */
        list = g_hash_table_lookup (headers, "Cache-Control");
        if (list) {
                GSList *l;
                int res;

                res = 0;

                for (l = list; l; l = l->next) {
                        res = sscanf (l->data,
                                      "max-age=%d",
                                      &timeout);
                        if (res == 1)
                                break;
                }

                if (res != 1) {
                        g_warning ("Invalid 'Cache-Control' header. Assuming "
                                   "default max-age of %d.\n"
                                   "Header was:\n%s",
                                   SSDP_DEFAULT_MAX_AGE,
                                   (char *) list->data);

                        timeout = SSDP_DEFAULT_MAX_AGE;
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
                                           SSDP_DEFAULT_MAX_AGE,
                                           (char *) list->data);

                                timeout = SSDP_DEFAULT_MAX_AGE;
                        }
                } else {
                        g_warning ("No 'Cache-Control' nor any 'Expires' "
                                   "header was specified. Assuming default "
                                   "max-age of %d.", SSDP_DEFAULT_MAX_AGE);

                        timeout = SSDP_DEFAULT_MAX_AGE;
                }
        }

        resource->timeout_id = g_timeout_add (timeout * 1000,
                                              resource_expire,
                                              resource);

        /* Only continue with signal emission if this resource was not
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
        g_signal_emit (resource_browser,
                       signals[RESOURCE_AVAILABLE],
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
resource_unavailable (GSSDPResourceBrowser *resource_browser,
                      GHashTable           *headers)
{
        GSList *list;
        const char *usn;

        list = g_hash_table_lookup (headers, "USN");
        if (!list)
                return; /* No USN specified */
        usn = list->data;

        /* Only process if we were cached */
        if (!g_hash_table_lookup (resource_browser->priv->resources, usn))
                return;

        g_signal_emit (resource_browser,
                       signals[RESOURCE_UNAVAILABLE],
                       0,
                       usn);

        g_hash_table_remove (resource_browser->priv->resources, usn);
}

static void
received_discovery_response (GSSDPResourceBrowser *resource_browser,
                             GHashTable           *headers)
{
        GSList *list;

        list = g_hash_table_lookup (headers, "ST");
        if (!list)
                return; /* No target specified */

        if (strcmp (resource_browser->priv->target, list->data) != 0)
                return; /* Target doesn't match */

        resource_available (resource_browser, headers);
}

static void
received_announcement (GSSDPResourceBrowser *resource_browser,
                       GHashTable           *headers)
{
        GSList *list;

        list = g_hash_table_lookup (headers, "NT");
        if (!list)
                return; /* No target specified */

        if (strcmp (resource_browser->priv->target, list->data) != 0)
                return; /* Target doesn't match */

        list = g_hash_table_lookup (headers, "NTS");
        if (!list)
                return; /* No announcement type specified */

        /* Check announcement type */
        if      (strncmp (list->data,
                          SSDP_ALIVE_NTS,
                          strlen (SSDP_ALIVE_NTS)) == 0)
                resource_available (resource_browser, headers);
        else if (strncmp (list->data,
                          SSDP_BYEBYE_NTS,
                          strlen (SSDP_BYEBYE_NTS)) == 0)
                resource_unavailable (resource_browser, headers);
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
        GSSDPResourceBrowser *resource_browser;

        resource_browser = GSSDP_RESOURCE_BROWSER (user_data);

        if (!resource_browser->priv->active)
                return;

        switch (type) {
        case _GSSDP_DISCOVERY_RESPONSE:
                received_discovery_response (resource_browser, headers);
                break;
        case _GSSDP_ANNOUNCEMENT:
                received_announcement (resource_browser, headers);
                break;
        default:
                break;
        }
}

/**
 * Free a Resource structure and its contained data
 **/
static void
resource_free (gpointer data)
{
        Resource *resource;

        resource = data;

        g_free (resource->usn);

        g_source_remove (resource->timeout_id);

        g_slice_free (Resource, resource);
}

static gboolean
clear_cache_helper (gpointer key, gpointer value, gpointer data)
{
        Resource *resource;

        resource = value;

        g_signal_emit (resource->resource_browser,
                       signals[RESOURCE_UNAVAILABLE],
                       0,
                       resource->usn);

        return TRUE;
}

/**
 * Clears the cached resources hash
 **/
static void
clear_cache (GSSDPResourceBrowser *resource_browser)
{
        /* Clear cache */
        g_hash_table_foreach_remove (resource_browser->priv->resources,
                                     clear_cache_helper,
                                     NULL);
}
