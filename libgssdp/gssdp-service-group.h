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

#ifndef __GSSDP_SERVICE_GROUP_H__
#define __GSSDP_SERVICE_GROUP_H__

#include "gssdp-client.h"

G_BEGIN_DECLS

GType
gssdp_service_group_get_type (void) G_GNUC_CONST;

#define GSSDP_TYPE_SERVICE_GROUP \
                (gssdp_service_group_get_type ())
#define GSSDP_SERVICE_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GSSDP_TYPE_SERVICE_GROUP, \
                 GSSDPServiceGroup))
#define GSSDP_SERVICE_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 GSSDP_TYPE_SERVICE_GROUP, \
                 GSSDPServiceGroupClass))
#define GSSDP_IS_SERVICE_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GSSDP_TYPE_SERVICE_GROUP))
#define GSSDP_IS_SERVICE_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 GSSDP_TYPE_SERVICE_GROUP))
#define GSSDP_SERVICE_GROUP_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GSSDP_TYPE_SERVICE_GROUP, \
                 GSSDPServiceGroupClass))

typedef struct _GSSDPServiceGroupPrivate GSSDPServiceGroupPrivate;

typedef struct {
        GObject parent;

        GSSDPServiceGroupPrivate *priv;
} GSSDPServiceGroup;

typedef struct {
        GObjectClass parent_class;

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
} GSSDPServiceGroupClass;

GSSDPServiceGroup *
gssdp_service_group_new                (GSSDPClient       *client);

GSSDPClient *
gssdp_service_group_get_client         (GSSDPServiceGroup *service_group);

void
gssdp_service_group_set_max_age        (GSSDPServiceGroup *service_group,
                                        guint              max_age);

guint
gssdp_service_group_get_max_age        (GSSDPServiceGroup *service_group);

void
gssdp_service_group_set_available      (GSSDPServiceGroup *service_group,
                                        gboolean           available);

gboolean
gssdp_service_group_get_available      (GSSDPServiceGroup *service_group);

guint
gssdp_service_group_add_service        (GSSDPServiceGroup *service_group,
                                        const char        *target,
                                        const char        *usn,
                                        GList             *locations);

guint
gssdp_service_group_add_service_simple (GSSDPServiceGroup *service_group,
                                        const char        *target,
                                        const char        *usn,
                                        const char        *location);

void
gssdp_service_group_remove_service     (GSSDPServiceGroup *service_group,
                                        guint              service_id);

G_END_DECLS

#endif /* __GSSDP_SERVICE_GROUP_H__ */
