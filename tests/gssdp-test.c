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

#include <libgssdp/gssdp.h>

static void
service_available_cb (GSSDPServiceBrowser *service_browser,
                      const char          *usn,
                      GList               *locations)
{
        GList *l;

        g_print ("service available\n"
                 "  USN:      %s\n",
                 usn);
        
        for (l = locations; l; l = l->next)
                g_print ("  Location: %s\n", (char *) l->data);
}

static void
service_unavailable_cb (GSSDPServiceBrowser *service_browser,
                        const char          *usn)
{
        g_print ("service unavailable\n"
                 "  USN:      %s\n",
                 usn);
}

int
main (int    argc,
      char **argv)
{
        GSSDPClient *client;
        GSSDPServiceBrowser *service_browser;
        GError *error;
        GMainLoop *main_loop;

        g_type_init ();

        error = NULL;
        client = gssdp_client_new (NULL, &error);
        if (!client) {
                g_critical (error->message);

                g_error_free (error);

                return 1;
        }

        service_browser = gssdp_service_browser_new (client,
                                                     "upnp:rootdevice");

        g_signal_connect (service_browser,
                          "service-available",
                          G_CALLBACK (service_available_cb),
                          NULL);
        g_signal_connect (service_browser,
                          "service-unavailable",
                          G_CALLBACK (service_unavailable_cb),
                          NULL);

        error = NULL;
        if (!gssdp_service_browser_start (service_browser, &error)) {
                g_critical (error->message);

                g_error_free (error);
        }

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (service_browser);
        g_object_unref (client);

        return 0;
}
