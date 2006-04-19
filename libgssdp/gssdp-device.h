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

#include <glib-object.h>

#include "gssdp-discoverable.h"

#ifndef __GSSDP_DEVICE_H__
#define __GSSDP_DEVICE_H__

G_BEGIN_DECLS

GType
gssdp_device_type (void) G_GNUC_CONST;

#define GSSDP_TYPE_DEVICE \
                (gssdp_device_type ())
#define GSSDP_DEVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GSSDP_TYPE_DEVICE, \
                 GSSDPDevice))
#define GSSDP_DEVICE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 GSSDP_TYPE_DEVICE, \
                 GSSDPDeviceClass))
#define GSSDP_IS_DEVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GSSDP_TYPE_DEVICE))
#define GSSDP_IS_DEVICE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 GSSDP_TYPE_DEVICE))
#define GSSDP_DEVICE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GSSDP_TYPE_DEVICE, \
                 GSSDPDeviceClass))

typedef struct _GSSDPDevicePrivate GSSDPDevicePrivate;

typedef struct {
        GSSDPDiscoverable parent;

        GSSDPDevicePrivate *priv;
} GSSDPDevice;

typedef struct {
        GSSDPDiscoverableClass parent_class;

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
} GSSDPDeviceClass;

#include "gssdp-root-device.h"
#include "gssdp-service.h"

GSSDPDevice *
gssdp_device_new          (GSSDPRootDevice *parent,
                           const char      *type,
                           gushort          version);

const char *
gssdp_device_get_uuid     (GSSDPDevice     *device);

const GList *
gssdp_device_get_services (GSSDPDevice     *divice);

G_END_DECLS

#endif /* __GSSDP_DEVICE_H__ */
