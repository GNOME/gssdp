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

#ifndef __GSSDP_DISCOVERABLE_H__
#define __GSSDP_DISCOVERABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

GType
gssdp_discoverable_type (void) G_GNUC_CONST;

#define GSSDP_TYPE_DISCOVERABLE \
                (gssdp_discoverable_type ())
#define GSSDP_DISCOVERABLE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GSSDP_TYPE_DISCOVERABLE, \
                 GSSDPDiscoverable))
#define GSSDP_DISCOVERABLE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 GSSDP_TYPE_DISCOVERABLE, \
                 GSSDPDiscoverableClass))
#define GSSDP_IS_DISCOVERABLE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GSSDP_TYPE_DISCOVERABLE))
#define GSSDP_IS_DISCOVERABLE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 GSSDP_TYPE_DISCOVERABLE))
#define GSSDP_DISCOVERABLE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GSSDP_TYPE_DISCOVERABLE, \
                 GSSDPDiscoverableClass))

typedef struct {
        GObject parent;

        /* future padding */
        gpointer _gssdp_reserved;
} GSSDPDiscoverable;

typedef struct {
        GObjectClass parent_class;

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
} GSSDPDiscoverableClass;

#include "gssdp-device.h"

const char *
gssdp_discoverable_get_type      (GSSDPDiscoverable *discoverable);

gushort
gssdp_discoverable_get_version   (GSSDPDiscoverable *discoverable);

GSSDPDevice *
gssdp_discoverable_get_parent    (GSSDPDiscoverable *discoverable);

void
gssdp_discoverable_set_available (GSSDPDiscoverable *discoverable,
                                  gboolean           available);

gboolean
gssdp_discoverable_get_available (GSSDPDiscoverable *discoverable);

G_END_DECLS

#endif /* __GSSDP_DISCOVERABLE_H__ */
