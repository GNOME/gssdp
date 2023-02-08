/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#ifndef GSSDP_RESOURCE_GROUP_H
#define GSSDP_RESOURCE_GROUP_H

#include <libgssdp/gssdp-client.h>
#include <libgssdp/gssdp-macros.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define GSSDP_TYPE_RESOURCE_GROUP (gssdp_resource_group_get_type ())
_GSSDP_API
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

_GSSDP_API
GSSDPResourceGroup *
gssdp_resource_group_new                 (GSSDPClient        *client);

_GSSDP_API
GSSDPClient *
gssdp_resource_group_get_client          (GSSDPResourceGroup *resource_group);

_GSSDP_API
void
gssdp_resource_group_set_max_age         (GSSDPResourceGroup *resource_group,
                                          guint               max_age);

_GSSDP_API
guint
gssdp_resource_group_get_max_age         (GSSDPResourceGroup *resource_group);

_GSSDP_API
void
gssdp_resource_group_set_available       (GSSDPResourceGroup *resource_group,
                                          gboolean            available);

_GSSDP_API
gboolean
gssdp_resource_group_get_available       (GSSDPResourceGroup *resource_group);

_GSSDP_API
void
gssdp_resource_group_set_message_delay         (GSSDPResourceGroup *resource_group,
                                                guint               message_delay);

_GSSDP_API
guint
gssdp_resource_group_get_message_delay         (GSSDPResourceGroup *resource_group);

_GSSDP_API
guint
gssdp_resource_group_add_resource        (GSSDPResourceGroup *resource_group,
                                          const char         *target,
                                          const char         *usn,
                                          GList              *locations);

_GSSDP_API
guint
gssdp_resource_group_add_resource_simple (GSSDPResourceGroup *resource_group,
                                          const char         *target,
                                          const char         *usn,
                                          const char         *location);

_GSSDP_API
void
gssdp_resource_group_remove_resource     (GSSDPResourceGroup *resource_group,
                                          guint               resource_id);

_GSSDP_API
void
gssdp_resource_group_update              (GSSDPResourceGroup *resource_group,
                                          guint               new_boot_id);

G_END_DECLS

#endif /* GSSDP_RESOURCE_GROUP_H */
