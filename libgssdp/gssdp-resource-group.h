/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
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

#ifndef GSSDP_RESOURCE_GROUP_H
#define GSSDP_RESOURCE_GROUP_H

#include <libgssdp/gssdp-client.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define GSSDP_TYPE_RESOURCE_GROUP (gssdp_resource_group_get_type ())
G_DECLARE_DERIVABLE_TYPE (GSSDPResourceGroup,
                          gssdp_resource_group,
                          GSSDP,
                          RESOURCE_GROUP,
                          GObject)

struct _GSSDPResourceGroupClass {
        GObjectClass parent_class;

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
};
typedef struct _GSSDPResourceGroupClass GSSDPResourceGroupClass;

GSSDPResourceGroup *
gssdp_resource_group_new                 (GSSDPClient        *client);

GSSDPClient *
gssdp_resource_group_get_client          (GSSDPResourceGroup *resource_group);

void
gssdp_resource_group_set_max_age         (GSSDPResourceGroup *resource_group,
                                          guint               max_age);

guint
gssdp_resource_group_get_max_age         (GSSDPResourceGroup *resource_group);

void
gssdp_resource_group_set_available       (GSSDPResourceGroup *resource_group,
                                          gboolean            available);

gboolean
gssdp_resource_group_get_available       (GSSDPResourceGroup *resource_group);

void
gssdp_resource_group_set_message_delay         (GSSDPResourceGroup *resource_group,
                                                guint               message_delay);

guint
gssdp_resource_group_get_message_delay         (GSSDPResourceGroup *resource_group);

guint
gssdp_resource_group_add_resource        (GSSDPResourceGroup *resource_group,
                                          const char         *target,
                                          const char         *usn,
                                          GList              *locations);

guint
gssdp_resource_group_add_resource_simple (GSSDPResourceGroup *resource_group,
                                          const char         *target,
                                          const char         *usn,
                                          const char         *location);

void
gssdp_resource_group_remove_resource     (GSSDPResourceGroup *resource_group,
                                          guint               resource_id);

void
gssdp_resource_group_update              (GSSDPResourceGroup *resource_group,
                                          guint               new_boot_id);

G_END_DECLS

#endif /* GSSDP_RESOURCE_GROUP_H */
