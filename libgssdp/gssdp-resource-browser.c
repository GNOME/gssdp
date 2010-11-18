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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gssdp-resource-browser
 * @short_description: Class handling resource discovery.
 *
 * #GUPnPResourceBrowser handles resource discovery. After creating a browser
 * and activating it, the ::resource-available and ::resource-unavailable
 * signals will be emitted whenever the availability of a resource matching the
 * specified discovery target changes. A discovery request is sent out
 * automatically when activating the browser.
 */

#include <config.h>
#include <libsoup/soup.h>
#include <string.h>
#include <stdio.h>

#include "gssdp-resource-browser.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"
#include "gssdp-marshal.h"

#define MAX_DISCOVERY_MESSAGES 3
#define DISCOVERY_FREQUENCY    500 /* 500 ms */

G_DEFINE_TYPE (GSSDPResourceBrowser,
               gssdp_resource_browser,
               G_TYPE_OBJECT);

struct _GSSDPResourceBrowserPrivate {
        GSSDPClient *client;

        char        *target;
        GRegex      *target_regex;

        gushort      mx;

        gboolean     active;

        gulong       message_received_id;

        GHashTable  *resources;
                        
        GSource     *timeout_src;
        guint        num_discovery;
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
        GSource              *timeout_src;
} Resource;

/* Function prototypes */
static void
gssdp_resource_browser_set_client (GSSDPResourceBrowser *resource_browser,
                                   GSSDPClient          *client);
static void
message_received_cb              (GSSDPClient          *client,
                                  const char           *from_ip,
                                  gushort               from_port,
                                  _GSSDPMessageType     type,
                                  SoupMessageHeaders   *headers,
                                  gpointer              user_data);
static void
resource_free                    (gpointer              data);
static void
clear_cache                      (GSSDPResourceBrowser *resource_browser);
static void
send_discovery_request            (GSSDPResourceBrowser *resource_browser);
static gboolean
discovery_timeout                (gpointer              data);
static void
start_discovery                  (GSSDPResourceBrowser *resource_browser);
static void
stop_discovery                   (GSSDPResourceBrowser *resource_browser);

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

                stop_discovery (resource_browser);

                g_object_unref (resource_browser->priv->client);
                resource_browser->priv->client = NULL;
        }

        clear_cache (resource_browser);

        G_OBJECT_CLASS (gssdp_resource_browser_parent_class)->dispose (object);
}

static void
gssdp_resource_browser_finalize (GObject *object)
{
        GSSDPResourceBrowser *resource_browser;

        resource_browser = GSSDP_RESOURCE_BROWSER (object);

        if (resource_browser->priv->target_regex)
                g_regex_unref (resource_browser->priv->target_regex);

        g_free (resource_browser->priv->target);

        g_hash_table_destroy (resource_browser->priv->resources);

        G_OBJECT_CLASS (gssdp_resource_browser_parent_class)->finalize (object);
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

        /**
         * GSSDPResourceBrowser:client
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
         * GSSDPResourceBrowser:target
         *
         * The discovery target.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_TARGET,
                 g_param_spec_string
                         ("target",
                          "Target",
                          "The discovery target.",
                          NULL,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPResourceBrowser:mx
         *
         * The maximum number of seconds in which to request other parties
         * to respond.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_MX,
                 g_param_spec_uint
                         ("mx",
                          "MX",
                          "The maximum number of seconds in which to request "
                          "other parties to respond.",
                          1,
                          G_MAXUSHORT,
                          SSDP_DEFAULT_MX,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GSSDPResourceBrowser:active
         *
         * Whether this browser is active or not.
         **/
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

        /**
         * GSSDPResourceBrowser::resource-available
         * @resource_browser: The #GSSDPResourceBrowser that received the
         * signal
         * @usn: The USN of the discovered resource
         * @locations: A #GList of strings describing the locations of the
         * discovered resource.
         *
         * The ::resource-available signal is emitted whenever a new resource
         * has become available.
         **/
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

        /**
         * GSSDPResourceBrowser::resource-unavailable
         * @resource_browser: The #GSSDPResourceBrowser that received the
         * signal
         * @usn: The USN of the resource
         *
         * The ::resource-unavailable signal is emitted whenever a resource
         * is not available any more.
         **/
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
        char *pattern;
        char *version;
        char *version_pattern;
        GError *error;

        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));
        g_return_if_fail (target != NULL);
        g_return_if_fail (!resource_browser->priv->active);
        
        g_free (resource_browser->priv->target);
        resource_browser->priv->target = g_strdup (target);

        if (resource_browser->priv->target_regex)
                g_regex_unref (resource_browser->priv->target_regex);

        version_pattern = "[0-9]+";
        /* Make sure we have enough room for version pattern */
        pattern = g_strndup (target,
                             strlen (target) + strlen (version_pattern));

        version = g_strrstr (pattern, ":") + 1;
        if (version != NULL &&
            g_regex_match_simple (version_pattern, version, 0, 0)) {
                strcpy (version, version_pattern);
        }

        error = NULL;
        resource_browser->priv->target_regex = g_regex_new (pattern,
                                                            0,
                                                            0,
                                                            &error);
        if (error) {
                g_warning ("Error compiling regular expression '%s': %s",
                           pattern,
                           error->message);

                g_error_free (error);
        }

        g_free (pattern);
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
                start_discovery (resource_browser);
        } else {
                stop_discovery (resource_browser);

                clear_cache (resource_browser);
        }
        
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
        char *usn;

        resource = user_data;

        /* Steal the USN pointer from the resource as we need it for the signal
         * emission.
         */
        usn = resource->usn;
        resource->usn = NULL;

        g_hash_table_remove (resource->resource_browser->priv->resources, usn);

        g_signal_emit (resource->resource_browser,
                       signals[RESOURCE_UNAVAILABLE],
                       0,
                       usn);
        g_free (usn);

        return FALSE;
}

static void
resource_available (GSSDPResourceBrowser *resource_browser,
                    SoupMessageHeaders   *headers)
{
        const char *usn;
        const char *header;
        Resource *resource;
        gboolean was_cached;
        guint timeout;
        GList *locations;
        GMainContext *context;

        usn = soup_message_headers_get_one (headers, "USN");
        if (!usn)
                return; /* No USN specified */

        /* Get from cache, if possible */
        resource = g_hash_table_lookup (resource_browser->priv->resources, usn);
        if (resource) {
                /* Remove old timeout */
                g_source_destroy (resource->timeout_src);

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
        header = soup_message_headers_get_one (headers, "Cache-Control");
        if (header) {
                GSList *list;
                int res;

                res = 0;

                for (list = soup_header_parse_list (header);
                     list;
                     list = list->next) {
                        res = sscanf (list->data,
                                      "max-age = %d",
                                      &timeout);
                        if (res == 1)
                                break;
                }

                if (res != 1) {
                        g_warning ("Invalid 'Cache-Control' header. Assuming "
                                   "default max-age of %d.\n"
                                   "Header was:\n%s",
                                   SSDP_DEFAULT_MAX_AGE,
                                   header);

                        timeout = SSDP_DEFAULT_MAX_AGE;
                }

                soup_header_free_list (list);
        } else {
                const char *expires;

                expires = soup_message_headers_get_one (headers, "Expires");
                if (expires) {
                        SoupDate *soup_exp_time;
                        time_t exp_time, cur_time;

                        soup_exp_time = soup_date_new_from_string (expires);
                        exp_time = soup_date_to_time_t (soup_exp_time);
                        soup_date_free (soup_exp_time);

                        cur_time = time (NULL);

                        if (exp_time > cur_time)
                                timeout = exp_time - cur_time;
                        else {
                                g_warning ("Invalid 'Expires' header. Assuming "
                                           "default max-age of %d.\n"
                                           "Header was:\n%s",
                                           SSDP_DEFAULT_MAX_AGE,
                                           expires);

                                timeout = SSDP_DEFAULT_MAX_AGE;
                        }
                } else {
                        g_warning ("No 'Cache-Control' nor any 'Expires' "
                                   "header was specified. Assuming default "
                                   "max-age of %d.", SSDP_DEFAULT_MAX_AGE);

                        timeout = SSDP_DEFAULT_MAX_AGE;
                }
        }

        resource->timeout_src = g_timeout_source_new_seconds (timeout);
	g_source_set_callback (resource->timeout_src,
                               resource_expire,
			       resource, NULL);

        context = gssdp_client_get_main_context
                (resource_browser->priv->client);
	g_source_attach (resource->timeout_src, context);

        g_source_unref (resource->timeout_src);

        /* Only continue with signal emission if this resource was not
         * cached already */
        if (was_cached)
                return;

        /* Build list of locations */
        locations = NULL;

        header = soup_message_headers_get_one (headers, "Location");
        if (header)
                locations = g_list_append (locations, g_strdup (header));

        header = soup_message_headers_get_one (headers, "AL");
        if (header) {
                /* Parse AL header. The format is:
                 * <uri1><uri2>... */
                const char *start, *end;
                char *uri;
                
                start = header;
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
                      SoupMessageHeaders   *headers)
{
        const char *usn;

        usn = soup_message_headers_get_one (headers, "USN");
        if (!usn)
                return; /* No USN specified */

        /* Only process if we were cached */
        if (!g_hash_table_lookup (resource_browser->priv->resources, usn))
                return;

        g_hash_table_remove (resource_browser->priv->resources, usn);

        g_signal_emit (resource_browser,
                       signals[RESOURCE_UNAVAILABLE],
                       0,
                       usn);
}

static gboolean
check_target_compat (GSSDPResourceBrowser *resource_browser,
                     const char           *st)
{
        return strcmp (resource_browser->priv->target,
                       GSSDP_ALL_RESOURCES) == 0 ||
               g_regex_match (resource_browser->priv->target_regex,
                              st,
                              0,
                              NULL);
}

static void
received_discovery_response (GSSDPResourceBrowser *resource_browser,
                             SoupMessageHeaders   *headers)
{
        const char *st;

        st = soup_message_headers_get_one (headers, "ST");
        if (!st)
                return; /* No target specified */

        if (!check_target_compat (resource_browser, st))
                return; /* Target doesn't match */

        resource_available (resource_browser, headers);
}

static void
received_announcement (GSSDPResourceBrowser *resource_browser,
                       SoupMessageHeaders   *headers)
{
        const char *header;

        header = soup_message_headers_get_one (headers, "NT");
        if (!header)
                return; /* No target specified */

        if (!check_target_compat (resource_browser, header))
                return; /* Target doesn't match */

        header = soup_message_headers_get_one (headers, "NTS");
        if (!header)
                return; /* No announcement type specified */

        /* Check announcement type */
        if      (strncmp (header,
                          SSDP_ALIVE_NTS,
                          strlen (SSDP_ALIVE_NTS)) == 0)
                resource_available (resource_browser, headers);
        else if (strncmp (header,
                          SSDP_BYEBYE_NTS,
                          strlen (SSDP_BYEBYE_NTS)) == 0)
                resource_unavailable (resource_browser, headers);
}

/**
 * Received a message
 **/
static void
message_received_cb (GSSDPClient        *client,
                     const char         *from_ip,
                     gushort             from_port,
                     _GSSDPMessageType   type,
                     SoupMessageHeaders *headers,
                     gpointer            user_data)
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

        g_source_destroy (resource->timeout_src);

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

/* Sends discovery request */
static void
send_discovery_request (GSSDPResourceBrowser *resource_browser)
{
        char *message;

        message = g_strdup_printf (SSDP_DISCOVERY_REQUEST,
                                   resource_browser->priv->target,
                                   resource_browser->priv->mx,
                                   g_get_application_name () ?: "");

        _gssdp_client_send_message (resource_browser->priv->client,
                                    NULL,
                                    0,
                                    message);

        g_free (message);
}

static gboolean
discovery_timeout (gpointer data)
{
        GSSDPResourceBrowser *resource_browser;

        resource_browser = GSSDP_RESOURCE_BROWSER (data);

        send_discovery_request (resource_browser);

        resource_browser->priv->num_discovery += 1;

        if (resource_browser->priv->num_discovery >= MAX_DISCOVERY_MESSAGES) {
		resource_browser->priv->timeout_src = NULL;
                resource_browser->priv->num_discovery = 0;

                return FALSE;
        } else
                return TRUE;
}

/* Starts sending discovery requests */
static void
start_discovery (GSSDPResourceBrowser *resource_browser)
{
        GMainContext *context;

        /* Send one now */
        send_discovery_request (resource_browser);

        /* And schedule the rest for later */
        resource_browser->priv->num_discovery = 1;
        resource_browser->priv->timeout_src =
                g_timeout_source_new (DISCOVERY_FREQUENCY);
	g_source_set_callback (resource_browser->priv->timeout_src,
			       discovery_timeout,
			       resource_browser, NULL);

        context = gssdp_client_get_main_context
                (resource_browser->priv->client);
	g_source_attach (resource_browser->priv->timeout_src, context);

        g_source_unref (resource_browser->priv->timeout_src);
}

/* Stops the sending of discovery messages */
static void
stop_discovery (GSSDPResourceBrowser *resource_browser)
{
	if (resource_browser->priv->timeout_src) {
		g_source_destroy (resource_browser->priv->timeout_src);
		resource_browser->priv->timeout_src = NULL;
		resource_browser->priv->num_discovery = 0;
	}
}
