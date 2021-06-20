/*
 * Copyright (C) 2012 Openismus GmbH
 *
 * Author: Jens Georg <jensg@openismus.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include "test-util.h"

G_GNUC_NORETURN void
on_resource_unavailable_assert_not_reached (G_GNUC_UNUSED GSSDPResourceBrowser *src,
                                            G_GNUC_UNUSED const char           *usn,
                                            G_GNUC_UNUSED gpointer              user_data)
{
        g_assert_not_reached ();
}

G_GNUC_NORETURN void
on_resource_available_assert_not_reached (G_GNUC_UNUSED GSSDPResourceBrowser *src,
                                          G_GNUC_UNUSED const char           *usn,
                                          G_GNUC_UNUSED GList                *locations,
                                          G_GNUC_UNUSED gpointer              user_data)
{
        g_assert_not_reached ();
}

gboolean
quit_loop (gpointer user_data)
{
        g_main_loop_quit ((GMainLoop *) user_data);

        return FALSE;
}

gboolean
unref_object (gpointer object)
{
        g_object_unref ((GObject *) object);

        return FALSE;
}

GSSDPClient *
get_client (GError **outer_error)
{
        static gsize init_guard = 0;
        static char *device = NULL;

        if (g_once_init_enter (&init_guard)) {
                GSSDPClient *client = NULL;
                GError *error = NULL;

                g_debug ("Detecting network interface to use for tests...");

                client = gssdp_client_new ("lo", &error);
                if (error == NULL) {
                        g_debug ("Using lo");
                        device = g_strdup ("lo");
                        g_object_unref (client);
                } else {
                        g_clear_error(&error);
                        client = gssdp_client_new ("lo0", &error);
                        if (error == NULL) {
                                g_debug ("Using lo0");
                                device = g_strdup ("lo0");
                                g_object_unref (client);
                        } else {
                                g_debug ("Using default interface, expect fails");
                        }
                }
                g_once_init_leave (&init_guard, 1);
        }

        return gssdp_client_new (device, outer_error);
}
