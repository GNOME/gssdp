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

#ifndef __GSSDP_SERVICE_BROWSER_H__
#define __GSSDP_SERVICE_BROWSER_H__

#include "gssdp-client.h"

G_BEGIN_DECLS

GType
gssdp_service_browser_get_type (void) G_GNUC_CONST;

#define GSSDP_TYPE_SERVICE_BROWSER \
                (gssdp_service_browser_get_type ())
#define GSSDP_SERVICE_BROWSER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GSSDP_TYPE_SERVICE_BROWSER, \
                 GSSDPServiceBrowser))
#define GSSDP_SERVICE_BROWSER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 GSSDP_TYPE_SERVICE_BROWSER, \
                 GSSDPServiceBrowserClass))
#define GSSDP_IS_SERVICE_BROWSER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GSSDP_TYPE_SERVICE_BROWSER))
#define GSSDP_IS_SERVICE_BROWSER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 GSSDP_TYPE_SERVICE_BROWSER))
#define GSSDP_SERVICE_BROWSER_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GSSDP_TYPE_SERVICE_BROWSER, \
                 GSSDPServiceBrowserClass))

typedef struct _GSSDPServiceBrowserPrivate GSSDPServiceBrowserPrivate;

typedef struct {
        GObject parent;

        GSSDPServiceBrowserPrivate *priv;
} GSSDPServiceBrowser;

typedef struct {
        GObjectClass parent_class;

        /* signals */
        void (* service_available)   (GSSDPServiceBrowser *service_browser,
                                      const char          *usn,
                                      const GList         *locations);
        
        void (* service_unavailable) (GSSDPServiceBrowser *service_browser,
                                      const char          *usn);

        /* future padding */
        void (* _gssdp_reserved1) (void);
        void (* _gssdp_reserved2) (void);
        void (* _gssdp_reserved3) (void);
        void (* _gssdp_reserved4) (void);
} GSSDPServiceBrowserClass;

GSSDPServiceBrowser *
gssdp_service_browser_new        (GSSDPClient         *client,
                                  const char          *target);

GSSDPClient *
gssdp_service_browser_get_client (GSSDPServiceBrowser *service_browser);

void
gssdp_service_browser_set_target (GSSDPServiceBrowser *service_browser,
                                  const char          *target);

const char *
gssdp_service_browser_get_target (GSSDPServiceBrowser *service_browser);

void
gssdp_service_browser_set_mx     (GSSDPServiceBrowser *service_browser,
                                  gushort              mx);

gushort
gssdp_service_browser_get_mx     (GSSDPServiceBrowser *service_browser);

gboolean
gssdp_service_browser_set_active (GSSDPServiceBrowser *service_browser,
                                  gboolean             active,
                                  GError             **error);

gboolean
gssdp_service_browser_get_active (GSSDPServiceBrowser *service_browser);

G_END_DECLS

#endif /* __GSSDP_SERVICE_BROWSER_H__ */
