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

int
main (int    argc,
      char **argv)
{
        GSSDPClient *client;
        GSSDPResourceGroup *resource_group;
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
