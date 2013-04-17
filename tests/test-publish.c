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

int
main (G_GNUC_UNUSED int    argc,
      G_GNUC_UNUSED char **argv)
{
        GSSDPClient *client;
        GSSDPResourceGroup *resource_group;
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

        resource_group = gssdp_resource_group_new (client);

        gssdp_resource_group_add_resource_simple
                (resource_group,
                 "upnp:rootdevice",
                 "uuid:1234abcd-12ab-12ab-12ab-1234567abc12::upnp:rootdevice",
                 "http://192.168.1.100/");

        gssdp_resource_group_set_available (resource_group, TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (resource_group);
        g_object_unref (client);

        return EXIT_SUCCESS;
}
