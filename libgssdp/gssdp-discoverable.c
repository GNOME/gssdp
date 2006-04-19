/* 
 * (C) 2006 OpenedHand Ltd.
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

#include "gssdp-discoverable.h"

/* Hack around G_DEFINE_TYPE hardcoding the type function name */
#define gssdp_discoverable_get_type gssdp_discoverable_type

G_DEFINE_ABSTRACT_TYPE (GSSDPDiscoverable,
                        gssdp_discoverable,
                        G_TYPE_OBJECT);

#undef gssdp_discoverable_get_type

struct _GSSDPDiscoverablePrivate {
        char        *type;
        gushort      version;

        GSSDPDevice *parent;

        gboolean     available;
};

enum {
        PROP_0,
        PROP_TYPE,
        PROP_VERSION,
        PROP_PARENT,
        PROP_AVAILABLE
};

/* Function prototypes */
static void
gssdp_discoverable_set_type    (GSSDPDiscoverable *discoverable,
                                const char        *type);
static void
gssdp_discoverable_set_version (GSSDPDiscoverable *discoverable,
                                gushort            version);
static void
gssdp_discoverable_set_parent  (GSSDPDiscoverable *discoverable,
                                GSSDPDevice       *parent);

static void
gssdp_discoverable_init (GSSDPDiscoverable *discoverable)
{
        discoverable->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (discoverable,
                                         GSSDP_TYPE_DISCOVERABLE,
                                         GSSDPDiscoverablePrivate);
}

static void
gssdp_discoverable_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        GSSDPDiscoverable *discoverable;

        discoverable = GSSDP_DISCOVERABLE (object);

        switch (property_id) {
        case PROP_TYPE:
                gssdp_discoverable_set_type
                        (discoverable, g_value_get_string (value));
                break;
        case PROP_VERSION:
                gssdp_discoverable_set_version
                        (discoverable, g_value_get_uint (value));
                break;
        case PROP_PARENT:
                gssdp_discoverable_set_parent
                        (discoverable, g_value_get_object (value));
                break;
        case PROP_AVAILABLE:
                gssdp_discoverable_set_available
                        (discoverable, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_discoverable_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
        GSSDPDiscoverable *discoverable;

        discoverable = GSSDP_DISCOVERABLE (object);

        switch (property_id) {
        case PROP_TYPE:
                g_value_set_string
                        (value,
                         gssdp_discoverable_get_type (discoverable));
                break;
        case PROP_VERSION:
                g_value_set_uint
                        (value,
                         gssdp_discoverable_get_version (discoverable));
                break;
        case PROP_PARENT:
                g_value_set_object
                        (value,
                         gssdp_discoverable_get_parent (discoverable));
                break;
        case PROP_AVAILABLE:
                g_value_set_boolean
                        (value,
                         gssdp_discoverable_get_available (discoverable));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gssdp_discoverable_dispose (GObject *object)
{
        GSSDPDiscoverable *discoverable;

        discoverable = GSSDP_DISCOVERABLE (object);

        if (discoverable->priv->parent) {
                g_object_unref (discoverable->priv->parent);
                discoverable->priv->parent = NULL;
        }
}

static void
gssdp_discoverable_finalize (GObject *object)
{
        GSSDPDiscoverable *discoverable;

        discoverable = GSSDP_DISCOVERABLE (object);

        g_free (discoverable->priv->type);
}

static void
gssdp_discoverable_class_init (GSSDPDiscoverableClass *klass)
{
        GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gssdp_discoverable_set_property;
	object_class->get_property = gssdp_discoverable_get_property;
        object_class->dispose      = gssdp_discoverable_dispose;
	object_class->finalize     = gssdp_discoverable_finalize;

        g_type_class_add_private (klass, sizeof (GSSDPDiscoverablePrivate));

        g_object_class_install_property
                (object_class,
                 PROP_TYPE,
                 g_param_spec_string
                         ("type",
                          "Type",
                          "The device or service type.",
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_VERSION,
                 g_param_spec_uint
                         ("version",
                          "Version",
                          "The device or service type version.",
                          1,
                          G_MAXUSHORT,
                          1,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_PARENT,
                 g_param_spec_object
                         ("parent",
                          "Parent",
                          "The parent device.",
                          GSSDP_TYPE_DEVICE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        g_object_class_install_property
                (object_class,
                 PROP_AVAILABLE,
                 g_param_spec_boolean
                         ("available",
                          "Available",
                          "TRUE if the discoverable is available.",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));
}

/**
 * Sets the device or service type of @discoverable to @type
 **/
static void
gssdp_discoverable_set_type (GSSDPDiscoverable *discoverable,
                             const char        *type)
{
        g_return_if_fail (type != NULL);

        discoverable->priv->type = g_strdup (type);

        g_object_notify (G_OBJECT (discoverable), "type");
}

/**
 * gssdp_discoverable_get_type
 * @discoverable: A #GSSDPDiscoverable
 *
 * Return value: The device or service.
 **/
const char *
gssdp_discoverable_get_type (GSSDPDiscoverable *discoverable)
{
        g_return_val_if_fail (GSSDP_IS_DISCOVERABLE (discoverable), NULL);

        return discoverable->priv->type;
}

/**
 * Sets the device or service type version of @discoverable to @version
 **/
static void
gssdp_discoverable_set_version (GSSDPDiscoverable *discoverable,
                                gushort            version)
{
        g_return_if_fail (version > 0);

        discoverable->priv->version = version;

        g_object_notify (G_OBJECT (discoverable), "version");
}

/**
 * gssdp_discoverable_get_version
 * @discoverable: A #GSSDPDiscoverable
 *
 * Return value: The device or service type version.
 **/
gushort
gssdp_discoverable_get_version (GSSDPDiscoverable *discoverable)
{
        g_return_val_if_fail (GSSDP_IS_DISCOVERABLE (discoverable), 0);
        
        return discoverable->priv->version;
}

/**
 * Sets the parent device of @discoverable to @parent
 **/
static void
gssdp_discoverable_set_parent (GSSDPDiscoverable *discoverable,
                               GSSDPDevice       *parent)
{
        g_return_if_fail (GSSDP_IS_DEVICE (parent) || parent == NULL);

        if (parent) {
                discoverable->priv->parent = g_object_ref (parent);

                g_object_notify (G_OBJECT (discoverable), "parent");
        }
}

/**
 * gssdp_discoverable_get_parent
 * @discoverable: A #GSSDPDiscoverable
 *
 * Return value: The parent #GSSDPDevice.
 **/
GSSDPDevice *
gssdp_discoverable_get_parent (GSSDPDiscoverable *discoverable)
{
        g_return_val_if_fail (GSSDP_IS_DISCOVERABLE (discoverable), NULL);
        
        return discoverable->priv->parent;
}

/**
 * gssdp_discoverable_set_available
 * @discoverable: A #GSSDPDiscoverable
 * @available: TRUE if @discoverable should be available
 *
 * If @available is TRUE, sets @discoverable to be available. 
 **/
void
gssdp_discoverable_set_available (GSSDPDiscoverable *discoverable,
                                  gboolean           available)
{
        g_return_if_fail (GSSDP_IS_DISCOVERABLE (discoverable));

        if (discoverable->priv->available != available) {
                discoverable->priv->available = available;

                g_object_notify (G_OBJECT (discoverable), "available");
        }
}

/**
 * gssdp_discoverable_get_available
 * @discoverable: A #GSSDPDiscoverable
 *
 * Return value: TRUE if @discoverable is available.
 **/
gboolean
gssdp_discoverable_get_available (GSSDPDiscoverable *discoverable)
{
        g_return_val_if_fail (GSSDP_IS_DISCOVERABLE (discoverable), FALSE);
        
        return discoverable->priv->available;
}
