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

#ifndef __GSSDP_ROOT_DEVICE_H__
#define __GSSDP_ROOT_DEVICE_H__

#include <glib-object.h>

#include "gssdp-device.h"

G_BEGIN_DECLS

GType
gssdp_root_device_type (void) G_GNUC_CONST;

#define GSSDP_TYPE_ROOT_DEVICE \
                (gssdp_root_device_type ())
#define GSSDP_ROOT_DEVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GSSDP_TYPE_ROOT_DEVICE, \
                 GSSDPRootDevice))
#define GSSDP_ROOT_DEVICE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 GSSDP_TYPE_ROOT_DEVICE, \
                 GSSDPRootDeviceClass))
#define GSSDP_IS_ROOT_DEVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GSSDP_TYPE_ROOT_DEVICE))
#define GSSDP_IS_ROOT_DEVICE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 GSSDP_TYPE_ROOT_DEVICE))
#define GSSDP_ROOT_DEVICE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GSSDP_TYPE_ROOT_DEVICE, \
                 GSSDPRootDeviceClass))

typedef struct _GSSDPRootDevicePrivate GSSDPRootDevicePrivate;

typedef struct {
        GSSDPDevice parent;

        GSSDPRootDevicePrivate *priv;
} GSSDPRootDevice;

typedef struct {
        GSSDPDeviceClass parent_class;

        /* signals */
        void (* discoverable_available)   (const char *target,
                                           const char *usn,
                                           const char *location);
        void (* discoverable_unavailable) (const char *target,
                                           const char *usn);

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
} GSSDPRootDeviceClass;

GSSDPRootDevice *
gssdp_root_device_new           (const char      *type,
                                 gushort          version,
                                 const char      *location);

const char *
gssdp_root_device_get_location  (GSSDPRootDevice *device);

void
gssdp_root_device_set_server_id (GSSDPRootDevice *device,
                                 const char      *server_id);

const char *
gssdp_root_device_get_server_id (GSSDPRootDevice *device);

const GList *
gssdp_root_device_get_devices   (GSSDPRootDevice *device);

void
gssdp_root_device_discover      (const char      *target);

G_END_DECLS

#endif /* __GSSDP_ROOT_DEVICE_H__ */
