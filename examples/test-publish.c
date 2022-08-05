/* 
 * Copyright (C) 2006 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
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

        error = NULL;
        client = gssdp_client_new_full (NULL,
                                        NULL,
                                        0,
                                        GSSDP_UDA_VERSION_1_0,
                                        &error);
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
