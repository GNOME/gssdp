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
#include <stdlib.h>

static void
resource_available_cb (GSSDPResourceBrowser *resource_browser,
                       const char           *usn,
                       GList                *locations)
{
        GList *l;

        g_print ("resource available\n"
                 "  USN:      %s\n",
                 usn);
        
        for (l = locations; l; l = l->next)
                g_print ("  Location: %s\n", (char *) l->data);
}

static void
resource_unavailable_cb (GSSDPResourceBrowser *resource_browser,
                         const char           *usn)
{
        g_print ("resource unavailable\n"
                 "  USN:      %s\n",
                 usn);
}

int
main (int    argc,
      char **argv)
{
        GSSDPClient *client;
        GSSDPResourceBrowser *resource_browser;
        GError *error;
        GMainLoop *main_loop;

        g_type_init ();

        error = NULL;
        client = gssdp_client_new (NULL, NULL, &error);
        if (error) {
                g_printerr ("Error creating the GSSDP client: %s\n",
                            error->message);

                g_error_free (error);

                return EXIT_FAILURE;
        }

        resource_browser = gssdp_resource_browser_new (client,
                                                       GSSDP_ALL_RESOURCES);

        g_signal_connect (resource_browser,
                          "resource-available",
                          G_CALLBACK (resource_available_cb),
                          NULL);
        g_signal_connect (resource_browser,
                          "resource-unavailable",
                          G_CALLBACK (resource_unavailable_cb),
                          NULL);

        gssdp_resource_browser_set_active (resource_browser, TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (resource_browser);
        g_object_unref (client);

        return EXIT_SUCCESS;
}
