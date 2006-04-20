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

/* XXX connect to "notify::available" and handle actual advertisement */

#include <config.h>

#include "gssdp-service.h"

/* Hack around G_DEFINE_TYPE hardcoding the type function name */
#define gssdp_service_get_type gssdp_service_type

G_DEFINE_TYPE (GSSDPService,
               gssdp_service,
               GSSDP_TYPE_DISCOVERABLE);

#undef gssdp_service_get_type

struct _GSSDPServicePrivate {
};

static void
gssdp_service_init (GSSDPService *service)
{
        service->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (service,
                                         GSSDP_TYPE_SERVICE,
                                         GSSDPServicePrivate);
}

static void
gssdp_service_class_init (GSSDPServiceClass *klass)
{
        GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

        g_type_class_add_private (klass, sizeof (GSSDPServicePrivate));
}

/**
 * gssdp_service_new
 * @parent: A #GSSDPDevice
 * @type: A string identifying the type of the service
 * @version: A #gushort identifying the version of the service type
 *
 * Return value: A new #GSSDPService object.
 **/
GSSDPService *
gssdp_service_new (GSSDPDevice *parent,
                   const char  *type,
                   gushort      version)
{
        return g_object_new (GSSDP_TYPE_SERVICE,
                             "parent",  parent,
                             "type",    type,
                             "version", version,
                             NULL);
}
