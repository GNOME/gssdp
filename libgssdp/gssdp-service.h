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

#ifndef __GSSDP_SERVICE_H__
#define __GSSDP_SERVICE_H__

#include <glib-object.h>

#include "gssdp-discoverable.h"
#include "gssdp-device.h"

G_BEGIN_DECLS

GType
gssdp_service_type (void) G_GNUC_CONST;

#define GSSDP_TYPE_SERVICE \
                (gssdp_service_type ())
#define GSSDP_SERVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GSSDP_TYPE_SERVICE, \
                 GSSDPService))
#define GSSDP_SERVICE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 GSSDP_TYPE_SERVICE, \
                 GSSDPServiceClass))
#define GSSDP_IS_SERVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GSSDP_TYPE_SERVICE))
#define GSSDP_IS_SERVICE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 GSSDP_TYPE_SERVICE))
#define GSSDP_SERVICE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GSSDP_TYPE_SERVICE, \
                 GSSDPServiceClass))

typedef struct _GSSDPServicePrivate GSSDPServicePrivate;

typedef struct {
        GSSDPDiscoverable parent;

        GSSDPServicePrivate *priv;
} GSSDPService;

typedef struct {
        GSSDPDiscoverableClass parent_class;

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
} GSSDPServiceClass;

GSSDPService *
gssdp_service_new (GSSDPDevice *parent,
                   const char  *type,
                   gushort      version);

G_END_DECLS

#endif /* __GSSDP_SERVICE_H__ */
