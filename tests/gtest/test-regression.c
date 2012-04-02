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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define UUID_1 "uuid:81909e94-ebf4-469e-ac68-81f2f189de1b"
#define USN "urn:org-gupnp:device:RegressionTest673150:2"
#define USN_1 "urn:org-gupnp:device:RegressionTest673150:1"

#include <libgssdp/gssdp-resource-browser.h>
#include <libgssdp/gssdp-resource-group.h>

#include "test-util.h"

static gboolean
on_test_bgo673150_delay_timeout (gpointer user_data)
{
        GSSDPResourceBrowser *browser = GSSDP_RESOURCE_BROWSER (user_data);

        gssdp_resource_browser_set_active (browser, TRUE);
        g_assert (gssdp_resource_browser_get_active (browser));

        return FALSE;
}

static void
test_bgo673150 (void)
{
        GSSDPClient *dest, *src;
        GSSDPResourceBrowser *browser;
        GSSDPResourceGroup *group;
        GError *error = NULL;
        GMainLoop *loop;
        gulong signal_id;

        dest = gssdp_client_new (NULL, "lo", &error);
        g_assert (dest != NULL);
        g_assert (error == NULL);

        src = gssdp_client_new (NULL, "lo", &error);
        g_assert (src != NULL);
        g_assert (error == NULL);

        group = gssdp_resource_group_new (src);
        gssdp_resource_group_add_resource_simple (group,
                                                  USN,
                                                  UUID_1"::"USN,
                                                  "http://127.0.0.1:3456");
        gssdp_resource_group_set_max_age (group, 10);

        browser = gssdp_resource_browser_new (dest, USN_1);

        signal_id = g_signal_connect (browser,
                                      "resource-unavailable",
                                      G_CALLBACK (on_resource_unavailable_assert_not_reached),
                                      NULL);

        /* Delay resource browser until ressource group sent its initial
         * announcement */
        g_timeout_add_seconds (5, on_test_bgo673150_delay_timeout, browser);

        gssdp_resource_group_set_available (group, TRUE);

        g_assert (gssdp_resource_group_get_available (group));

        loop = g_main_loop_new (NULL, FALSE);
        g_timeout_add_seconds (30, (GSourceFunc) g_main_loop_quit, loop);
        g_main_loop_run (loop);

        /* prevent the _unref from triggering the assertion */
        g_signal_handler_disconnect (browser, signal_id);
        g_object_unref (group);
        g_object_unref (browser);
        g_object_unref (src);
        g_object_unref (dest);
        g_main_loop_unref (loop);
}

int main (int argc, char *argv[])
{
        g_type_init ();
        g_test_init (&argc, &argv, NULL);

        if (g_test_slow ()) {
               g_test_add_func ("/bugs/gnome/673150",
                                test_bgo673150);
        }

        g_test_run ();

        return 0;
}
