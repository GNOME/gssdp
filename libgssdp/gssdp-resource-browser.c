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
 * SECTION:gssdp-resource-browser
 * @short_description: Class handling resource discovery.
 *
 * #GSSDPResourceBrowser handles resource discovery. After creating a browser
 * and activating it, the ::resource-available and ::resource-unavailable
 * signals will be emitted whenever the availability of a resource matching the
 * specified discovery target changes. A discovery request is sent out
 * automatically when activating the browser.
 */

#include <config.h>
#include <libsoup/soup.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "gssdp-resource-browser.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"
#include "gssdp-marshal.h"

#define RESCAN_TIMEOUT 5 /* 5 seconds */
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
        guint        version;

        GSource     *refresh_cache_src;
        GHashTable  *fresh_resources;
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
        GList                *locations;
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
resource_free                    (Resource             *data);
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
static gboolean
refresh_cache                    (gpointer data);
static void
resource_unavailable             (GSSDPResourceBrowser *resource_browser,
                                  SoupMessageHeaders   *headers);

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
                                       g_free,
                                       (GFreeFunc) resource_free);
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
         * GSSDPResourceBrowser:client:
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
         * GSSDPResourceBrowser:target:
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
         * GSSDPResourceBrowser:mx:
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
         * GSSDPResourceBrowser:active:
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
         * GSSDPResourceBrowser::resource-available:
         * @resource_browser: The #GSSDPResourceBrowser that received the
         * signal
         * @usn: The USN of the discovered resource
         * @locations: (type GList*) (transfer none) (element-type utf8): A #GList of strings describing the locations of the
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
         * GSSDPResourceBrowser::resource-unavailable:
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
 * gssdp_resource_browser_new:
 * @client: The #GSSDPClient to associate with
 * @target: A SSDP search target
 *
 * @target is a generic string the resource browser listens for on the SSDP
 * bus. There are several possible targets such as
 * <itemizedlist>
 *   <listitem><para>"ssdp:all" for everything</para></listitem>
 *   <listitem><para>
 *     "upnp:rootdevice" for UPnP device entry points, not caring about the
 *     device type</para></listitem>
 *   <listitem><para>The UUID of a specific device</para></listitem>
 *   <listitem><para>Device types such as
 *   "urn:schemas-upnp-org:device:MediaServer:1"</para></listitem>
 *   <listitem><para>Service types such as
 *   "urn:schemas-upnp-org:service:ContentDirectory:1"</para></listitem>
 * </itemizedlist>
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

/*
 * Sets the #GSSDPClient @resource_browser is associated with to @client
 */
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
 * gssdp_resource_browser_get_client:
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Returns: (transfer none): The #GSSDPClient @resource_browser is associated with.
 **/
GSSDPClient *
gssdp_resource_browser_get_client (GSSDPResourceBrowser *resource_browser)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser),
                              NULL);

        return resource_browser->priv->client;
}

/**
 * gssdp_resource_browser_set_target:
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
        const char *version_pattern;
        GError *error;

        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));
        g_return_if_fail (target != NULL);
        g_return_if_fail (!resource_browser->priv->active);
        
        g_free (resource_browser->priv->target);
        resource_browser->priv->target = g_strdup (target);

        if (resource_browser->priv->target_regex)
                g_regex_unref (resource_browser->priv->target_regex);

        version_pattern = "([0-9]+)";
        /* Make sure we have enough room for version pattern */
        pattern = g_strndup (target,
                             strlen (target) + strlen (version_pattern));

        version = g_strrstr (pattern, ":");
        if (version != NULL &&
            (g_strstr_len (pattern, -1, "uuid:") != pattern ||
             version != g_strstr_len (pattern, -1, ":")) &&
            g_regex_match_simple (version_pattern,
                                  version + 1,
                                  G_REGEX_ANCHORED,
                                  G_REGEX_MATCH_ANCHORED)) {
                resource_browser->priv->version = atoi (version + 1);
                strcpy (version + 1, version_pattern);
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
 * gssdp_resource_browser_get_target:
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
 * gssdp_resource_browser_set_mx:
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
 * gssdp_resource_browser_get_mx:
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
 * gssdp_resource_browser_set_active:
 * @resource_browser: A #GSSDPResourceBrowser
 * @active: %TRUE to activate @resource_browser
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
 * gssdp_resource_browser_get_active:
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Return value: %TRUE if @resource_browser is active.
 **/
gboolean
gssdp_resource_browser_get_active (GSSDPResourceBrowser *resource_browser)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser), 0);

        return resource_browser->priv->active;
}

/**
 * gssdp_resource_browser_rescan:
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Begins discovery if @resource_browser is active and no discovery is
 * performed. Otherwise does nothing.
 *
 * Return value: %TRUE if rescaning has been started.
 **/
gboolean
gssdp_resource_browser_rescan (GSSDPResourceBrowser *resource_browser)
{
        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser), 0);

        if (resource_browser->priv->active &&
            resource_browser->priv->timeout_src == NULL &&
            resource_browser->priv->refresh_cache_src == NULL) {
                start_discovery (resource_browser);
                return TRUE;
        }

        return FALSE;
}

/*
 * Resource expired: Remove
 */
static gboolean
resource_expire (gpointer user_data)
{
        GSSDPResourceBrowser *resource_browser;
        Resource *resource;
        char *usn;
        char *canonical_usn;

        resource = user_data;
        resource_browser = resource->resource_browser;

        /* Steal the USN pointer from the resource as we need it for the signal
         * emission.
         */
        usn = resource->usn;
        resource->usn = NULL;

        if (resource_browser->priv->version > 0) {
                char *version;

                version = g_strrstr (usn, ":");
                canonical_usn = g_strndup (usn, version - usn);
        } else {
                canonical_usn = g_strdup (usn);
        }

        g_hash_table_remove (resource->resource_browser->priv->resources,
                             canonical_usn);

        g_signal_emit (resource_browser,
                       signals[RESOURCE_UNAVAILABLE],
                       0,
                       usn);
        g_free (usn);
        g_free (canonical_usn);

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
        gboolean destroyLocations;
        GList *it1, *it2;
        char *canonical_usn;

        usn = soup_message_headers_get_one (headers, "USN");
        if (!usn)
                return; /* No USN specified */

        /* Build list of locations */
        locations = NULL;
        destroyLocations = TRUE;

        header = soup_message_headers_get_one (headers, "Location");
        if (header) {
                GSocketFamily family;
                GSSDPClient *client;

                client = resource_browser->priv->client;
                family = gssdp_client_get_family (client);

                if (family == G_SOCKET_FAMILY_IPV6) {
                        SoupURI *uri = soup_uri_new (header);
                        const char *host = NULL;
                        GInetAddress *addr = NULL;

                        host = soup_uri_get_host (uri);
                        addr = g_inet_address_new_from_string (host);
                        if (g_inet_address_get_is_link_local (addr)) {
                                char *new_host;
                                int index = 0;

                                index = gssdp_client_get_index (client);

                                new_host = g_strdup_printf ("%s%%%d",
                                                            host,
                                                            index);
                                soup_uri_set_host (uri, new_host);
                        }
                        g_object_unref (addr);
                        locations = g_list_append (locations,
                                                   soup_uri_to_string (uri,
                                                                       FALSE));
                        soup_uri_free (uri);
                } else {
                        locations = g_list_append (locations, g_strdup (header));
                }
        }

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

        if (!locations)
                return; /* No location specified */

        if (resource_browser->priv->version > 0) {
                char *version;

                version = g_strrstr (usn, ":");
                canonical_usn = g_strndup (usn, version - usn);
        } else {
                canonical_usn = g_strdup (usn);
        }

        /* Get from cache, if possible */
        resource = g_hash_table_lookup (resource_browser->priv->resources,
                                        canonical_usn);
        /* Put usn into fresh resources, so this resource will not be
         * removed on cache refreshing. */
        if (resource_browser->priv->fresh_resources != NULL) {
                g_hash_table_add (resource_browser->priv->fresh_resources,
                                  g_strdup (canonical_usn));
        }

        /* If location does not match, expect that we missed bye bye packet */
        if (resource) {
                for (it1 = locations, it2 = resource->locations;
                     it1 && it2;
                     it1 = it1->next, it2 = it2->next) {
                        if (strcmp ((const char *) it1->data,
                                    (const char *) it2->data) != 0) {
                               resource_unavailable (resource_browser, headers);
                               /* Will be destroyed by resource_unavailable */
                               resource = NULL;

                               break;
                        }
                }
        }

        if (resource) {
                /* Remove old timeout */
                g_source_destroy (resource->timeout_src);

                was_cached = TRUE;
        } else {
                /* Create new Resource data structure */
                resource = g_slice_new (Resource);

                resource->resource_browser = resource_browser;
                resource->usn              = g_strdup (usn);
                resource->locations        = locations;
                destroyLocations = FALSE; /* Ownership passed to resource */
                
                g_hash_table_insert (resource_browser->priv->resources,
                                     canonical_usn,
                                     resource);
                
                was_cached = FALSE;

                /* hash-table takes ownership of this */
                canonical_usn = NULL;
        }

        if (canonical_usn != NULL)
                g_free (canonical_usn);

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

        g_source_attach (resource->timeout_src,
                         g_main_context_get_thread_default ());

        g_source_unref (resource->timeout_src);

        /* Only continue with signal emission if this resource was not
         * cached already */
        if (!was_cached) {
                /* Emit signal */
                g_signal_emit (resource_browser,
                               signals[RESOURCE_AVAILABLE],
                               0,
                               usn,
                               locations);
        }
        /* Cleanup */
        if (destroyLocations)
                g_list_free_full (locations, g_free);
}

static void
resource_unavailable (GSSDPResourceBrowser *resource_browser,
                      SoupMessageHeaders   *headers)
{
        const char *usn;
        char *canonical_usn;

        usn = soup_message_headers_get_one (headers, "USN");
        if (!usn)
                return; /* No USN specified */

        if (resource_browser->priv->version > 0) {
                char *version;

                version = g_strrstr (usn, ":");
                canonical_usn = g_strndup (usn, version - usn);
        } else {
                canonical_usn = g_strdup (usn);
        }

        /* Only process if we were cached */
        if (!g_hash_table_lookup (resource_browser->priv->resources,
                                  canonical_usn))
                goto out;

        g_hash_table_remove (resource_browser->priv->resources,
                             canonical_usn);

        g_signal_emit (resource_browser,
                       signals[RESOURCE_UNAVAILABLE],
                       0,
                       usn);

out:
        g_free (canonical_usn);
}

static gboolean
check_target_compat (GSSDPResourceBrowser *resource_browser,
                     const char           *st)
{
        GMatchInfo *info;
        int         version;
        char       *tmp;

        if (strcmp (resource_browser->priv->target,
                    GSSDP_ALL_RESOURCES) == 0)
                return TRUE;

        if (!g_regex_match (resource_browser->priv->target_regex,
                            st,
                            0,
                            &info)) {
                g_match_info_free (info);

                return FALSE;
        }

        /* If there was no version to match, we're done */
        if (resource_browser->priv->version == 0) {
                g_match_info_free (info);

                return TRUE;
        }

        if (g_match_info_get_match_count (info) != 2) {
                g_match_info_free (info);

                return FALSE;
        }

        version = atoi ((tmp = g_match_info_fetch (info, 1)));
        g_free (tmp);
        g_match_info_free (info);

        if (version < 0) {
            return FALSE;
        }

        return (guint) version >= resource_browser->priv->version;
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

/*
 * Received a message
 */
static void
message_received_cb (G_GNUC_UNUSED GSSDPClient *client,
                     G_GNUC_UNUSED const char  *from_ip,
                     G_GNUC_UNUSED gushort      from_port,
                     _GSSDPMessageType          type,
                     SoupMessageHeaders        *headers,
                     gpointer                   user_data)
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
        case _GSSDP_DISCOVERY_REQUEST:
                /* Should not happend */
                break;
        default:
                break;
        }
}

/*
 * Free a Resource structure and its contained data
 */
static void
resource_free (Resource *resource)
{
        g_free (resource->usn);
        g_source_destroy (resource->timeout_src);
        g_list_free_full (resource->locations, g_free);
        g_slice_free (Resource, resource);
}

static gboolean
clear_cache_helper (G_GNUC_UNUSED gpointer key,
                    gpointer               value,
                    G_GNUC_UNUSED gpointer data)
{
        Resource *resource;

        resource = value;

        g_signal_emit (resource->resource_browser,
                       signals[RESOURCE_UNAVAILABLE],
                       0,
                       resource->usn);

        return TRUE;
}

/*
 * Clears the cached resources hash
 */
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
        char *message = NULL;
        const char *group = NULL;
        char *dest = NULL;
        GSSDPClient *client = NULL;

        client = resource_browser->priv->client;
        group = _gssdp_client_get_mcast_group (client);

        /* FIXME: Check for IPv6 - ugly and remove strcpys */
        if (strchr (group, ':')) {
            dest = g_strdup_printf ("[%s]", group);
        } else {
            dest = g_strdup (group);
        }

        message = g_strdup_printf (SSDP_DISCOVERY_REQUEST,
                                   dest,
                                   resource_browser->priv->target,
                                   resource_browser->priv->mx,
                                   g_get_prgname () ? g_get_prgname () : "");

        _gssdp_client_send_message (resource_browser->priv->client,
                                    NULL,
                                    0,
                                    message,
                                    _GSSDP_DISCOVERY_REQUEST);

        g_free (dest);
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

                /* Setup cache refreshing */
                resource_browser->priv->refresh_cache_src =
                                  g_timeout_source_new_seconds (RESCAN_TIMEOUT);
                g_source_set_callback
                                     (resource_browser->priv->refresh_cache_src,
                                      refresh_cache,
                                      resource_browser,
                                      NULL);
                g_source_attach (resource_browser->priv->refresh_cache_src,
                                 g_main_context_get_thread_default ());
                g_source_unref (resource_browser->priv->refresh_cache_src);

                return FALSE;
        } else
                return TRUE;
}

/* Starts sending discovery requests */
static void
start_discovery (GSSDPResourceBrowser *resource_browser)
{
        /* Send one now */
        send_discovery_request (resource_browser);

        /* And schedule the rest for later */
        resource_browser->priv->num_discovery = 1;
        resource_browser->priv->timeout_src =
                g_timeout_source_new (DISCOVERY_FREQUENCY);
        g_source_set_callback (resource_browser->priv->timeout_src,
                               discovery_timeout,
                               resource_browser, NULL);

        g_source_attach (resource_browser->priv->timeout_src,
                         g_main_context_get_thread_default ());

        g_source_unref (resource_browser->priv->timeout_src);

        /* Setup a set of responsive resources for cache refreshing */
        resource_browser->priv->fresh_resources = g_hash_table_new_full
                                        (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         NULL);
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
        if (resource_browser->priv->refresh_cache_src) {
                g_source_destroy (resource_browser->priv->refresh_cache_src);
                resource_browser->priv->refresh_cache_src = NULL;
        }
        if (resource_browser->priv->fresh_resources) {
                g_hash_table_unref (resource_browser->priv->fresh_resources);
                resource_browser->priv->fresh_resources = NULL;
        }
}

static gboolean
refresh_cache_helper (gpointer key, gpointer value, gpointer data)
{
        Resource *resource;
        GHashTable *fresh_resources;

        resource = value;
        fresh_resources = data;

        if (g_hash_table_contains (fresh_resources, key))
                return FALSE;
        else {
                g_signal_emit (resource->resource_browser,
                               signals[RESOURCE_UNAVAILABLE],
                               0,
                               resource->usn);

                return TRUE;
        }
}

/* Removes non-responsive resources */
static gboolean
refresh_cache (gpointer data)
{
        GSSDPResourceBrowser *resource_browser;

        resource_browser = GSSDP_RESOURCE_BROWSER (data);
        g_hash_table_foreach_remove (resource_browser->priv->resources,
                                     refresh_cache_helper,
                                     resource_browser->priv->fresh_resources);
        g_hash_table_unref (resource_browser->priv->fresh_resources);
        resource_browser->priv->fresh_resources = NULL;
        resource_browser->priv->refresh_cache_src = NULL;

        return FALSE;
}
