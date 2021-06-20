/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#ifndef GSSDP_RESOURCE_BROWSER_H
#define GSSDP_RESOURCE_BROWSER_H

#include "gssdp-client.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GSSDP_TYPE_RESOURCE_BROWSER (gssdp_resource_browser_get_type ())

G_DECLARE_DERIVABLE_TYPE (GSSDPResourceBrowser,
                          gssdp_resource_browser,
                          GSSDP,
                          RESOURCE_BROWSER,
                          GObject)

struct _GSSDPResourceBrowserClass {
        GObjectClass parent_class;

        /* signals */
        void (* resource_available)   (GSSDPResourceBrowser *resource_browser,
                                       const char           *usn,
                                       const GList          *locations);

        void (* resource_update)      (GSSDPResourceBrowser *resource_browser,
                                       const char           *usn,
                                       guint                 boot_id,
                                       guint                 next_boot_id);

        void (* resource_unavailable) (GSSDPResourceBrowser *resource_browser,
                                       const char           *usn);

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
};

/**
 * GSSDP_ALL_RESOURCES:
 *
 * SSDP search target for finding all possible resources.
 **/
#define GSSDP_ALL_RESOURCES "ssdp:all"

GSSDPResourceBrowser *
gssdp_resource_browser_new        (GSSDPClient          *client,
                                   const char           *target);

GSSDPClient *
gssdp_resource_browser_get_client (GSSDPResourceBrowser *resource_browser);

void
gssdp_resource_browser_set_target (GSSDPResourceBrowser *resource_browser,
                                   const char           *target);

const char *
gssdp_resource_browser_get_target (GSSDPResourceBrowser *resource_browser);

void
gssdp_resource_browser_set_mx     (GSSDPResourceBrowser *resource_browser,
                                   gushort               mx);

gushort
gssdp_resource_browser_get_mx     (GSSDPResourceBrowser *resource_browser);

void
gssdp_resource_browser_set_active (GSSDPResourceBrowser *resource_browser,
                                   gboolean              active);

gboolean
gssdp_resource_browser_get_active (GSSDPResourceBrowser *resource_browser);

gboolean
gssdp_resource_browser_rescan     (GSSDPResourceBrowser *resource_browser);

G_END_DECLS

#endif /* GSSDP_RESOURCE_BROWSER_H */
