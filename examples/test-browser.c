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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <libgssdp/gssdp.h>
#include <gio/gio.h>
#include <stdlib.h>

static void
resource_available_cb (G_GNUC_UNUSED GSSDPResourceBrowser *resource_browser,
                       const char                         *usn,
                       GList                              *locations)
{
        GList *l;

        g_print ("resource available\n"
                 "  USN:      %s\n",
                 usn);
        
        for (l = locations; l; l = l->next)
                g_print ("  Location: %s\n", (char *) l->data);
}

static void
resource_unavailable_cb (G_GNUC_UNUSED GSSDPResourceBrowser *resource_browser,
                         const char                         *usn)
{
        g_print ("resource unavailable\n"
                 "  USN:      %s\n",
                 usn);
}

int
main (G_GNUC_UNUSED int    argc,
      G_GNUC_UNUSED char **argv)
{
        GSSDPClient *client;
        GSSDPResourceBrowser *resource_browser;
        GError *error;
        GMainLoop *main_loop;

#if !GLIB_CHECK_VERSION (2, 35, 0)
        g_type_init ();
#endif

        error = NULL;
        client = g_initable_new (GSSDP_TYPE_CLIENT,
                                 NULL,
                                 &error,
                                 NULL);
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
