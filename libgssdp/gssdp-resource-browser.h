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

#ifndef __GSSDP_RESOURCE_BROWSER_H__
#define __GSSDP_RESOURCE_BROWSER_H__

#include "gssdp-client.h"

G_BEGIN_DECLS

GType
gssdp_resource_browser_get_type (void) G_GNUC_CONST;

#define GSSDP_TYPE_RESOURCE_BROWSER \
                (gssdp_resource_browser_get_type ())
#define GSSDP_RESOURCE_BROWSER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GSSDP_TYPE_RESOURCE_BROWSER, \
                 GSSDPResourceBrowser))
#define GSSDP_RESOURCE_BROWSER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 GSSDP_TYPE_RESOURCE_BROWSER, \
                 GSSDPResourceBrowserClass))
#define GSSDP_IS_RESOURCE_BROWSER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GSSDP_TYPE_RESOURCE_BROWSER))
#define GSSDP_IS_RESOURCE_BROWSER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 GSSDP_TYPE_RESOURCE_BROWSER))
#define GSSDP_RESOURCE_BROWSER_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GSSDP_TYPE_RESOURCE_BROWSER, \
                 GSSDPResourceBrowserClass))

typedef struct _GSSDPResourceBrowserPrivate GSSDPResourceBrowserPrivate;
typedef struct _GSSDPResourceBrowser GSSDPResourceBrowser;
typedef struct _GSSDPResourceBrowserClass GSSDPResourceBrowserClass;

struct _GSSDPResourceBrowser {
        GObject parent;

        GSSDPResourceBrowserPrivate *priv;
};

struct _GSSDPResourceBrowserClass {
        GObjectClass parent_class;

        /* signals */
        void (* resource_available)   (GSSDPResourceBrowser *resource_browser,
                                       const char           *usn,
                                       const GList          *locations);
        
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

#endif /* __GSSDP_RESOURCE_BROWSER_H__ */
