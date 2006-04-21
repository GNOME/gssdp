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

#include "gssdp-service-browser.h"
#include "gssdp-error.h"
#include "gssdp-client-private.h"
#include "gssdp-protocol.h"
#include "gssdp-marshal.h"

/* An MX of 3 seconds by default */
#define DEFAULT_MX 3

G_DEFINE_TYPE (GSSDPServiceBrowser,
               gssdp_service_browser,
               G_TYPE_OBJECT);

struct _GSSDPServiceBrowserPrivate {
        GSSDPClient *client;

        char        *target;

        gushort      mx;
};

enum {
        PROP_0,
        PROP_CLIENT,
        PROP_TARGET,
        PROP_MX
};

enum {
        SERVICE_AVAILABLE,
        SERVICE_UNAVAILABLE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Function prototypes */
static void
gssdp_service_browser_set_client (GSSDPServiceBrowser *service_browser,
                                  GSSDPClient         *client);
static void
gssdp_service_browser_set_target (GSSDPServiceBrowser *service_browser,
                                  const char          *target);

static void
gssdp_service_browser_init (GSSDPServiceBrowser *service_browser)
{
        service_browser->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (service_browser,
                                         GSSDP_TYPE_SERVICE_BROWSER,
                                         GSSDPServiceBrowserPrivate);

        service_browser->priv->mx = DEFAULT_MX;
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
                g_object_unref (service_browser->priv->client);
                service_browser->priv->client = NULL;
        }
}

static void
gssdp_service_browser_finalize (GObject *object)
{
        GSSDPServiceBrowser *service_browser;

        service_browser = GSSDP_SERVICE_BROWSER (object);

        g_free (service_browser->priv->target);
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
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
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
                          DEFAULT_MX,
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
 * @main_context: The #GMainContext to associate with
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
 * @server_id: The server ID
 *
 * Sets the browser target of @service_browser to @target.
 **/
static void
gssdp_service_browser_set_target (GSSDPServiceBrowser *service_browser,
                                  const char          *target)
{
        g_return_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser));
        g_return_if_fail (target != NULL);
        
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

        if (service_browser->priv->mx != mx) {
                service_browser->priv->mx = mx;
                
                g_object_notify (G_OBJECT (service_browser), "mx");
        }
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
 * gssdp_service_browser_start
 * @service_browse: A #GSSDPServiceBrowser
 * @error:  A location to return an error of type #GSSDP_ERROR_QUARK
 *
 * Start service discovery.
 *
 * Return value: TRUE if service discovery was started successfully.
 **/
gboolean
gssdp_service_browser_start (GSSDPServiceBrowser *service_browser,
                             GError             **error)
{
        char *message;
        gboolean res;

        g_return_val_if_fail (GSSDP_IS_SERVICE_BROWSER (service_browser),
                              FALSE);

        message = g_strdup_printf (SSDP_DISCOVERY_REQUEST,
                                   service_browser->priv->target,
                                   service_browser->priv->mx);

        res = _gssdp_client_send_message (service_browser->priv->client,
                                          NULL,
                                          message,
                                          error);

        g_free (message);

        return res;
}
