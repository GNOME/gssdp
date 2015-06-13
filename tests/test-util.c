/*
 * Copyright (C) 2012 Openismus GmbH
 *
 * Author: Jens Georg <jensg@openismus.com>
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

                client = gssdp_client_new (NULL, "lo", &error);
                if (error == NULL) {
                        g_debug ("Using lo");
                        device = g_strdup ("lo");
                        g_object_unref (client);
                } else {
                        g_clear_error(&error);
                        client = gssdp_client_new (NULL, "lo0", &error);
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

        return gssdp_client_new (NULL, device, outer_error);
}
