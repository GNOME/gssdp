/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>

#include "gssdp-resource-browser.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"

#include <libsoup/soup.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define RESCAN_TIMEOUT 5 /* 5 seconds */
#define MAX_DISCOVERY_MESSAGES 3
#define DISCOVERY_FREQUENCY    500 /* 500 ms */

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
typedef struct _GSSDPResourceBrowserPrivate GSSDPResourceBrowserPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GSSDPResourceBrowser,
                            gssdp_resource_browser,
                            G_TYPE_OBJECT)

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
        RESOURCE_UPDATE,
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
        GSSDPResourceBrowserPrivate *priv = NULL;
        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        priv->mx = SSDP_DEFAULT_MX;

        priv->resources =
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
        GSSDPResourceBrowserPrivate *priv;

        resource_browser = GSSDP_RESOURCE_BROWSER (object);
        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        if (priv->client) {
                if (g_signal_handler_is_connected
                        (priv->client,
                         priv->message_received_id)) {
                        g_signal_handler_disconnect
                                (priv->client,
                                 priv->message_received_id);
                }

                stop_discovery (resource_browser);

                g_object_unref (priv->client);
                priv->client = NULL;
        }

        clear_cache (resource_browser);

        G_OBJECT_CLASS (gssdp_resource_browser_parent_class)->dispose (object);
}

static void
gssdp_resource_browser_finalize (GObject *object)
{
        GSSDPResourceBrowser *resource_browser;
        GSSDPResourceBrowserPrivate *priv;

        resource_browser = GSSDP_RESOURCE_BROWSER (object);
        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        g_clear_pointer (&priv->target_regex, g_regex_unref);

        g_free (priv->target);

        g_hash_table_destroy (priv->resources);

        G_OBJECT_CLASS (gssdp_resource_browser_parent_class)->finalize (object);

}

/**
 * GSSDPResourceBrowser:
 *
 * Class handling resource discovery.
 *
 * After creating a browser
 * and activating it, the [signal@GSSDP.ResourceBrowser::resource-available] and
 * [signal@GSSDP.ResourceBrowser::resource-unavailable] signals will be emitted
 * whenever the availability of a resource matching the specified discovery target
 * changes. A discovery request is sent out automatically when activating the browser.
 *
 * If the associated [class@GSSDP.Client] was configured to support UDA 1.1, it
 * will also emit the [signal@GSSDP.ResourceBrowser::resource-update] if any of
 * the UDA 1.1 devices on the nework annouced its upcoming BOOTID change.
 */
static void
gssdp_resource_browser_class_init (GSSDPResourceBrowserClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gssdp_resource_browser_set_property;
        object_class->get_property = gssdp_resource_browser_get_property;
        object_class->dispose      = gssdp_resource_browser_dispose;
        object_class->finalize     = gssdp_resource_browser_finalize;

        /**
         * GSSDPResourceBrowser:client:(attributes org.gtk.Property.get=gssdp_resource_browser_get_client):
         *
         * The [class@GSSDP.Client] to use for listening to SSDP messages
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
         * GSSDPResourceBrowser:target:(attributes org.gtk.Property.get=gssdp_resource_browser_get_target):
         *
         * The discovery target this resource browser is looking for.
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
         * GSSDPResourceBrowser:mx:(attributes org.gtk.Property.get=gssdp_resource_browser_get_mx org.gtk.Property.set=gssdp_resource_browser_set_mx):
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
         * GSSDPResourceBrowser:active:(attributes org.gtk.Property.get=gssdp_resource_browser_get_active org.gtk.Property.set=gssdp_resource_browser_set_active)
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
         * @locations: (type GList*) (transfer none) (element-type utf8): A [struct@GLib.List] of strings describing the locations of the
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
                              NULL, NULL, NULL,
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
                              NULL, NULL, NULL,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_STRING);

        /**
         * GSSDPResourceBrowser::resource-update:
         * @resource_browser: The #GSSDPResourceBrowser that received the
         * signal
         * @usn: The USN of the resource
         * @boot_id: The current boot-id
         * @next_boot_id : The next boot-id
         *
         * The ::resource-update signal is emitted whenever an UPnP 1.1
         * device is about to change it's BOOTID.
         *
         * Since: 1.2.0
         **/
        signals[RESOURCE_UPDATE] =
                g_signal_new ("resource-update",
                              GSSDP_TYPE_RESOURCE_BROWSER,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GSSDPResourceBrowserClass,
                                               resource_update),
                              NULL, NULL, NULL,
                              G_TYPE_NONE,
                              3,
                              G_TYPE_STRING,
                              G_TYPE_UINT,
                              G_TYPE_UINT);
}

/**
 * gssdp_resource_browser_new:
 * @client: The #GSSDPClient to associate with
 * @target: A SSDP search target
 *
 * Create a new resource browser for @target.
 *
 * @target is a generic string the resource browser listens for on the SSDP
 * bus. There are several possible targets such as
 *
 * - `ssdp:all` for everything that is announced using SSDP
 * - `upnp:rootdevice` for UPnP device entry points, not caring about
 *   a special device type
 * - The UUID of a specific device
 * - Device types, such as `urn:schemas-upnp-org:device:MediaServer:1`
 * - Service types, such as `urn:schemas-upnp-org:service:ContentDirectory:1`
 *
 * Return value: A new #GSSDPResourceBrowser object.
 */
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
        GSSDPResourceBrowserPrivate *priv;

        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));
        g_return_if_fail (GSSDP_IS_CLIENT (client));

        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        priv->client = g_object_ref (client);

        priv->message_received_id =
                g_signal_connect_object (priv->client,
                                         "message-received",
                                         G_CALLBACK (message_received_cb),
                                         resource_browser,
                                         0);

        g_object_notify (G_OBJECT (resource_browser), "client");
}

/**
 * gssdp_resource_browser_get_client:(attributes org.gtk.Method.get_property=client):
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Get the GSSDPClient this resource browser is using for SSDP.
 *
 * Returns: (transfer none): The #GSSDPClient @resource_browser is associated with.
 **/
GSSDPClient *
gssdp_resource_browser_get_client (GSSDPResourceBrowser *resource_browser)
{
        GSSDPResourceBrowserPrivate *priv;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser),
                              NULL);

        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        return priv->client;
}

/**
 * gssdp_resource_browser_set_target:(attributes org.gtk.Method.set_property=target):
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
        const char version_pattern[] = "([0-9]+)";
        GError *error;
        GSSDPResourceBrowserPrivate *priv;

        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));
        g_return_if_fail (target != NULL);

        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        g_return_if_fail (!priv->active);

        g_free (priv->target);
        priv->target = g_strdup (target);

        g_clear_pointer (&priv->target_regex, g_regex_unref);

        /* Make sure we have enough room for version pattern */
        pattern = g_strndup (target,
                             strlen (target) + sizeof (version_pattern));

        version = g_strrstr (pattern, ":");
        if (version != NULL &&
            (g_strstr_len (pattern, -1, "uuid:") != pattern ||
             version != g_strstr_len (pattern, -1, ":")) &&
            g_regex_match_simple (version_pattern,
                                  version + 1,
                                  G_REGEX_ANCHORED,
                                  G_REGEX_MATCH_ANCHORED)) {
                priv->version = atoi (version + 1);
                strncpy (version + 1, version_pattern, sizeof(version_pattern));
        }

        error = NULL;
        priv->target_regex = g_regex_new (pattern,
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
 * gssdp_resource_browser_get_target:(attributes org.gtk.Method.get_property=target):
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Get the current browse target.
 *
 * Return value: The browser target.
 **/
const char *
gssdp_resource_browser_get_target (GSSDPResourceBrowser *resource_browser)
{
        GSSDPResourceBrowserPrivate *priv;
        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser),
                              NULL);
        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        return priv->target;
}

/**
 * gssdp_resource_browser_set_mx:(attributes org.gtk.Method.set_property=mx):
 * @resource_browser: A #GSSDPResourceBrowser
 * @mx: The to be used MX value
 *
 * Sets the used MX value of @resource_browser to @mx.
 **/
void
gssdp_resource_browser_set_mx (GSSDPResourceBrowser *resource_browser,
                               gushort               mx)
{
        GSSDPResourceBrowserPrivate *priv;

        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));

        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        if (priv->mx == mx)
                return;

        priv->mx = mx;
        
        g_object_notify (G_OBJECT (resource_browser), "mx");
}

/**
 * gssdp_resource_browser_get_mx:(attributes org.gtk.Method.get_property=mx):
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Get the current MX value.
 *
 * Return value: The used MX value.
 **/
gushort
gssdp_resource_browser_get_mx (GSSDPResourceBrowser *resource_browser)
{
        GSSDPResourceBrowserPrivate *priv;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser), 0);

        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        return priv->mx;
}

/**
 * gssdp_resource_browser_set_active:(attributes org.gtk.Method.set_property=active):
 * @resource_browser: A #GSSDPResourceBrowser
 * @active: %TRUE to activate @resource_browser
 *
 * (De)activates @resource_browser.
 **/
void
gssdp_resource_browser_set_active (GSSDPResourceBrowser *resource_browser,
                                   gboolean              active)
{
        GSSDPResourceBrowserPrivate *priv;

        g_return_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser));

        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        if (priv->active == active)
                return;

        priv->active = active;

        if (active) {
                start_discovery (resource_browser);
        } else {
                stop_discovery (resource_browser);

                clear_cache (resource_browser);
        }
        
        g_object_notify (G_OBJECT (resource_browser), "active");
}

/**
 * gssdp_resource_browser_get_active:(attributes org.gtk.Method.get_property=active):
 * @resource_browser: A #GSSDPResourceBrowser
 *
 * Get whether the browser is currently active.
 *
 * Return value: %TRUE if @resource_browser is active.
 **/
gboolean
gssdp_resource_browser_get_active (GSSDPResourceBrowser *resource_browser)
{
        GSSDPResourceBrowserPrivate *priv;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser), 0);

        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        return priv->active;
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
        GSSDPResourceBrowserPrivate *priv;

        g_return_val_if_fail (GSSDP_IS_RESOURCE_BROWSER (resource_browser), 0);

        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        if (priv->active &&
            priv->timeout_src == NULL &&
            priv->refresh_cache_src == NULL) {
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
        GSSDPResourceBrowserPrivate *priv;
        Resource *resource;
        char *usn;
        char *canonical_usn;

        resource = user_data;
        resource_browser = resource->resource_browser;
        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        /* Steal the USN pointer from the resource as we need it for the signal
         * emission.
         */
        usn = g_steal_pointer (&resource->usn);

        if (priv->version > 0) {
                char *version;

                version = g_strrstr (usn, ":");
                canonical_usn = g_strndup (usn, version - usn);
        } else {
                canonical_usn = g_strdup (usn);
        }

        g_hash_table_remove (priv->resources, canonical_usn);

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
        GSSDPResourceBrowserPrivate *priv;
        const char *usn;
        const char *header;
        Resource *resource;
        gboolean was_cached;
        guint timeout;
        GList *locations;
        gboolean destroyLocations;
        GList *it1, *it2;
        char *canonical_usn;

        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        usn = soup_message_headers_get_one (headers, "USN");
        if (!usn)
                return; /* No USN specified */

        /* Build list of locations */
        locations = NULL;
        destroyLocations = TRUE;

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

        if (!locations)
                return; /* No location specified */

        if (priv->version > 0) {
                char *version;

                version = g_strrstr (usn, ":");
                canonical_usn = g_strndup (usn, version - usn);
        } else {
                canonical_usn = g_strdup (usn);
        }

        /* Get from cache, if possible */
        resource = g_hash_table_lookup (priv->resources,
                                        canonical_usn);
        /* Put usn into fresh resources, so this resource will not be
         * removed on cache refreshing. */
        if (priv->fresh_resources != NULL) {
                g_hash_table_add (priv->fresh_resources,
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
                
                g_hash_table_insert (priv->resources,
                                     canonical_usn,
                                     resource);
                
                was_cached = FALSE;

                /* hash-table takes ownership of this */
                canonical_usn = NULL;
        }

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
                        GDateTime *exp_time;

                        exp_time = soup_date_time_new_from_http_string (expires);
                        GDateTime *now = g_date_time_new_now_local ();

                        if (g_date_time_compare (now, exp_time) == 1)
                                timeout = g_date_time_difference (now, exp_time) / 1000 / 1000;
                        else {
                                g_warning ("Invalid 'Expires' header. Assuming "
                                           "default max-age of %d.\n"
                                           "Header was:\n%s",
                                           SSDP_DEFAULT_MAX_AGE,
                                           expires);

                                timeout = SSDP_DEFAULT_MAX_AGE;
                        }
                        g_date_time_unref (exp_time);
                        g_date_time_unref (now);
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
resource_update (GSSDPResourceBrowser *resource_browser,
                 SoupMessageHeaders   *headers)
{
        GSSDPResourceBrowserPrivate *priv;
        const char *usn;
        const char *boot_id_header;
        const char *next_boot_id_header;
        char *canonical_usn;
        guint boot_id;
        guint next_boot_id;
        gint64 out;

        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        usn = soup_message_headers_get_one (headers, "USN");
        boot_id_header = soup_message_headers_get_one (headers, "BOOTID.UPNP.ORG");
        next_boot_id_header = soup_message_headers_get_one (headers, "NEXTBOOTID.UPNP.ORG");

        if (!usn)
                return; /* No USN specified */

        if (!boot_id_header)
                return;

        if (!next_boot_id_header)
                return;

        if (!g_ascii_string_to_signed (boot_id_header, 10, 0, G_MAXINT32, &out, NULL))
                return;
        boot_id = out;

        if (!g_ascii_string_to_signed (next_boot_id_header, 10, 0, G_MAXINT32, &out, NULL))
                return;
        next_boot_id = out;

        if (priv->version > 0) {
                char *version;
                version = g_strrstr (usn, ":");
                canonical_usn = g_strndup (usn, version - usn);
        } else {
                canonical_usn = g_strdup (usn);
        }

        /* Only continue if we know about this. if not, there will be an
         * announcement afterwards anyway */
        if (!g_hash_table_lookup (priv->resources,
                                  canonical_usn))
                goto out;

        g_signal_emit (resource_browser,
                       signals[RESOURCE_UPDATE],
                       0,
                       usn,
                       boot_id,
                       next_boot_id);
out:
        g_free (canonical_usn);

}

static void
resource_unavailable (GSSDPResourceBrowser *resource_browser,
                      SoupMessageHeaders   *headers)
{
        GSSDPResourceBrowserPrivate *priv;
        const char *usn;
        char *canonical_usn;

        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        usn = soup_message_headers_get_one (headers, "USN");
        if (!usn)
                return; /* No USN specified */

        if (priv->version > 0) {
                char *version;

                version = g_strrstr (usn, ":");
                canonical_usn = g_strndup (usn, version - usn);
        } else {
                canonical_usn = g_strdup (usn);
        }

        /* Only process if we were cached */
        if (!g_hash_table_lookup (priv->resources,
                                  canonical_usn))
                goto out;

        g_hash_table_remove (priv->resources,
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
        GSSDPResourceBrowserPrivate *priv;
        GMatchInfo *info;
        int         version;
        char       *tmp;

        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        if (g_str_equal (priv->target, GSSDP_ALL_RESOURCES))
                return TRUE;

        if (!g_regex_match (priv->target_regex,
                            st,
                            0,
                            &info)) {
                g_match_info_free (info);

                return FALSE;
        }

        /* If there was no version to match, we're done */
        if (priv->version == 0) {
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

        return (guint) version >= priv->version;
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
        else if (strncmp (header,
                          SSDP_UPDATE_NTS,
                          strlen (SSDP_UPDATE_NTS)) == 0)
                resource_update (resource_browser, headers);
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
        GSSDPResourceBrowserPrivate *priv;

        resource_browser = GSSDP_RESOURCE_BROWSER (user_data);

        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        if (!priv->active)
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
        GSSDPResourceBrowserPrivate *priv;
        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        /* Clear cache */
        g_hash_table_foreach_remove (priv->resources,
                                     clear_cache_helper,
                                     NULL);
}

/* Sends discovery request */
static void
send_discovery_request (GSSDPResourceBrowser *resource_browser)
{
        GSSDPResourceBrowserPrivate *priv;
        char *message = NULL;
        const char *group = NULL;
        char *dest = NULL;

        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        group = _gssdp_client_get_mcast_group (priv->client);

        /* FIXME: Check for IPv6 - ugly and remove strcpys */
        if (strchr (group, ':')) {
            dest = g_strdup_printf ("[%s]", group);
        } else {
            dest = g_strdup (group);
        }

        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        message = g_strdup_printf (SSDP_DISCOVERY_REQUEST,
                                   dest,
                                   priv->target,
                                   priv->mx,
                                   gssdp_client_get_server_id (priv->client));

        _gssdp_client_send_message (priv->client,
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
        GSSDPResourceBrowserPrivate *priv;
        GSSDPResourceBrowser *resource_browser;

        resource_browser = GSSDP_RESOURCE_BROWSER (data);
        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        send_discovery_request (resource_browser);

        priv->num_discovery += 1;

        if (priv->num_discovery >= MAX_DISCOVERY_MESSAGES) {
                priv->timeout_src = NULL;
                priv->num_discovery = 0;

                /* Setup cache refreshing */
                priv->refresh_cache_src =
                                  g_timeout_source_new_seconds (RESCAN_TIMEOUT);
                g_source_set_callback
                                     (priv->refresh_cache_src,
                                      refresh_cache,
                                      resource_browser,
                                      NULL);
                g_source_attach (priv->refresh_cache_src,
                                 g_main_context_get_thread_default ());
                g_source_unref (priv->refresh_cache_src);

                return FALSE;
        } else
                return TRUE;
}

/* Starts sending discovery requests */
static void
start_discovery (GSSDPResourceBrowser *resource_browser)
{
        GSSDPResourceBrowserPrivate *priv;

        priv = gssdp_resource_browser_get_instance_private (resource_browser);

        /* Send one now */
        send_discovery_request (resource_browser);

        /* And schedule the rest for later */
        priv->num_discovery = 1;
        priv->timeout_src =
                g_timeout_source_new (DISCOVERY_FREQUENCY);
        g_source_set_callback (priv->timeout_src,
                               discovery_timeout,
                               resource_browser, NULL);

        g_source_attach (priv->timeout_src,
                         g_main_context_get_thread_default ());

        g_source_unref (priv->timeout_src);

        /* Setup a set of responsive resources for cache refreshing */
        priv->fresh_resources = g_hash_table_new_full
                                        (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         NULL);
}

/* Stops the sending of discovery messages */
static void
stop_discovery (GSSDPResourceBrowser *resource_browser)
{
        GSSDPResourceBrowserPrivate *priv;

        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        if (priv->timeout_src) {
                priv->num_discovery = 0;
        }

        g_clear_pointer (&priv->timeout_src, g_source_destroy);
        g_clear_pointer (&priv->refresh_cache_src, g_source_destroy);
        g_clear_pointer (&priv->fresh_resources, g_hash_table_destroy);
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
        GSSDPResourceBrowserPrivate *priv;

        resource_browser = GSSDP_RESOURCE_BROWSER (data);
        priv = gssdp_resource_browser_get_instance_private (resource_browser);
        g_hash_table_foreach_remove (priv->resources,
                                     refresh_cache_helper,
                                     priv->fresh_resources);
        g_hash_table_unref (priv->fresh_resources);
        priv->fresh_resources = NULL;
        priv->refresh_cache_src = NULL;

        return FALSE;
}
