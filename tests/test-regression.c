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

#define UUID_1 "uuid:81909e94-ebf4-469e-ac68-81f2f189de1b"
#define USN "urn:org-gupnp:device:RegressionTest673150:2"
#define USN_1 "urn:org-gupnp:device:RegressionTest673150:1"
#define NT_1 UUID_1"::"USN_1

#include <string.h>

#include <gio/gio.h>

#include <libgssdp/gssdp-resource-browser.h>
#include <libgssdp/gssdp-resource-group.h>
#include <libgssdp/gssdp-protocol.h>

#include "test-util.h"

/* Utility functions */

static GSocket *
create_socket(void)
{
        GSocket *socket;
        GError *error = NULL;
        GSocketAddress *sock_addr;
        GInetAddress *address;

        socket = g_socket_new (G_SOCKET_FAMILY_IPV4,
                               G_SOCKET_TYPE_DATAGRAM,
                               G_SOCKET_PROTOCOL_DEFAULT,
                               &error);
        g_assert (error == NULL);

        address = g_inet_address_new_from_string ("127.0.0.1");
        sock_addr = g_inet_socket_address_new (address, 0);
        g_object_unref (address);

        g_socket_bind (socket, sock_addr, TRUE, &error);
        g_assert (error == NULL);
        g_object_unref (sock_addr);

        return socket;
}

static char *
create_alive_message (const char *nt, int max_life)
{
        char *usn, *msg;

        if (strcmp (nt, UUID_1) == 0)
                usn = g_strdup (UUID_1);
        else
                usn = g_strconcat (UUID_1, "::", nt, NULL);

        msg = g_strdup_printf (SSDP_ALIVE_MESSAGE "\r\n",
                               SSDP_ADDR,
                               max_life,
                               "http://127.0.0.1:1234",
                               "",
                               "Linux/3.0 UPnP/1.0 GSSDPTesting/0.0.0",
                               nt,
                               usn);
        g_free (usn);

        return msg;
}


static gboolean
send_packet (gpointer user_data)
{
        GSocket *socket;
        GError *error = NULL;
        GSocketAddress *sock_addr;
        GInetAddress *address;
        char *msg = (char *) user_data;

        socket = create_socket ();

        address = g_inet_address_new_from_string (SSDP_ADDR);

        sock_addr = g_inet_socket_address_new (address, SSDP_PORT);
        g_object_unref (address);

        g_socket_send_to (socket, sock_addr, msg, strlen (msg), NULL, &error);
        g_assert (error == NULL);

        g_object_unref (sock_addr);
        g_object_unref (socket);

        g_free (msg);

        return FALSE;
}

/* BEGIN Regression test
 * https://bugzilla.gnome.org/show_bug.cgi?id=673150
 */

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

        dest = get_client (&error);
        g_assert (dest != NULL);
        g_assert (error == NULL);

        src = get_client (&error);
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

/* END Regression test
 * https://bugzilla.gnome.org/show_bug.cgi?id=673150
 * ============================================================================
 */

/* BEGIN Regression test
 * https://bugzilla.gnome.org/show_bug.cgi?id=682099
 * ============================================================================
 * - Start a resource browser and send a single SSDP packet with a lifetime of
 *   5 s.
 * - Check that there is a "resource-unavailable" signal.
 * - Shut down the ResourceBrowser and assert that there is NO
 *   "resource-unavailable" signal.
 */

static gboolean
announce_ressource_bgo682099 (gpointer user_data)
{
        send_packet (create_alive_message (USN_1, 5));

        return FALSE;
}

static void
resource_unavailabe_bgo682099 (GSSDPResourceBrowser *src,
                               const char           *usn,
                               gpointer              user_data)
{
        g_assert_cmpstr (usn, ==, NT_1);
        g_main_loop_quit ((GMainLoop *) user_data);
}

static void test_bgo682099 (void)
{
        GSSDPClient *dest;
        GSSDPResourceBrowser *browser;
        GError *error = NULL;
        GMainLoop *loop;
        gulong signal_id;

        loop = g_main_loop_new (NULL, FALSE);

        dest = get_client (&error);
        g_assert (dest != NULL);
        g_assert (error == NULL);

        browser = gssdp_resource_browser_new (dest, USN_1);
        signal_id = g_signal_connect (browser,
                                      "resource-unavailable",
                                      G_CALLBACK (resource_unavailabe_bgo682099),
                                      loop);
        gssdp_resource_browser_set_active (browser, TRUE);
        g_timeout_add_seconds (2, announce_ressource_bgo682099, NULL);
        g_main_loop_run (loop);
        g_signal_handler_disconnect (browser, signal_id);
        signal_id = g_signal_connect (browser,
                                      "resource-unavailable",
                                      G_CALLBACK (on_resource_unavailable_assert_not_reached),
                                      NULL);
        g_idle_add (unref_object, browser);
        g_timeout_add_seconds (10, quit_loop, loop);
        g_main_loop_run (loop);
}

/* END Regression test
 * https://bugzilla.gnome.org/show_bug.cgi?id=682099
 * ============================================================================
 */

/* BEGIN Regression test
 * https://bugzilla.gnome.org/show_bug.cgi?id=724030
 * ============================================================================
 * - Start a resource browser and send a two SSDP packets with different locations.
 * - Check that there are 2 "resource-unavailable" signals.
 * - Shut down the ResourceBrowser and assert that there is NO
 *   "resource-unavailable" signal.
 */
#define UUID_MISSED_BYE_BYE_1 "uuid:81909e94-ebf4-469e-ac68-81f2f18816ac"
#define USN_MISSED_BYE_BYE "urn:org-gupnp:device:RegressionTestMissedByeBye:2"
#define USN_MISSED_BYE_BYE_1 "urn:org-gupnp:device:RegressionTestMissedByeBye:1"
#define NT_MISSED_BYE_BYE_1 UUID_MISSED_BYE_BYE_1"::"USN_MISSED_BYE_BYE_1
#define LOCATION_MISSED_BYE_BYE_1 "http://127.0.0.1:1234"
#define LOCATION_MISSED_BYE_BYE_2 "http://127.0.0.1:1235"

static char *
create_alive_message_bgo724030 (const char *location)
{
        char *msg;

        msg = g_strdup_printf (SSDP_ALIVE_MESSAGE "\r\n",
                               SSDP_ADDR,
                               5,
                               location,
                               "",
                               "Linux/3.0 UPnP/1.0 GSSDPTesting/0.0.0",
                               NT_MISSED_BYE_BYE_1,
                               USN_MISSED_BYE_BYE_1);

        return msg;
}

static gboolean
announce_ressource_bgo724030_1 (gpointer user_data)
{
        send_packet (create_alive_message_bgo724030 (LOCATION_MISSED_BYE_BYE_1));

        return FALSE;
}

static gboolean
announce_ressource_bgo724030_2 (gpointer user_data)
{
        send_packet (create_alive_message_bgo724030 (LOCATION_MISSED_BYE_BYE_2));

        return FALSE;
}

static void
resource_availabe_bgo724030_1 (GSSDPResourceBrowser *src,
                               const char           *usn,
                               GList                *locations,
                               gpointer              user_data)
{
        g_assert_cmpstr (usn, ==, USN_MISSED_BYE_BYE_1);
        g_assert_cmpstr ((const char *) locations->data, ==, LOCATION_MISSED_BYE_BYE_1);
        g_main_loop_quit ((GMainLoop *) user_data);
}

static void
resource_availabe_bgo724030_2 (GSSDPResourceBrowser *src,
                               const char           *usn,
                               GList                *locations,
                               gpointer              user_data)
{
        g_assert_cmpstr (usn, ==, USN_MISSED_BYE_BYE_1);
        g_assert_cmpstr ((const char *) locations->data, ==, LOCATION_MISSED_BYE_BYE_2);
        g_main_loop_quit ((GMainLoop *) user_data);
}

static void
resource_unavailabe_bgo724030 (GSSDPResourceBrowser *src,
                               const char           *usn,
                               gpointer              user_data)
{
        g_assert_cmpstr (usn, ==, USN_MISSED_BYE_BYE_1);
        g_main_loop_quit ((GMainLoop *) user_data);
}

static void test_bgo724030 (void)
{
        GSSDPClient *dest;
        GSSDPResourceBrowser *browser;
        GError *error = NULL;
        GMainLoop *loop;
        gulong available_signal_id;

        loop = g_main_loop_new (NULL, FALSE);

        dest = get_client (&error);
        g_assert (dest != NULL);
        g_assert (error == NULL);

        browser = gssdp_resource_browser_new (dest, USN_MISSED_BYE_BYE_1);
        available_signal_id = g_signal_connect (browser,
                                      "resource-available",
                                      G_CALLBACK (resource_availabe_bgo724030_1),
                                      loop);
        g_signal_connect (browser,
                                      "resource-unavailable",
                                      G_CALLBACK (resource_unavailabe_bgo724030),
                                      loop);
        gssdp_resource_browser_set_active (browser, TRUE);
        g_timeout_add_seconds (2, announce_ressource_bgo724030_1, NULL);
        g_timeout_add_seconds (3, announce_ressource_bgo724030_2, NULL);
        g_main_loop_run (loop);  /* available */
        g_signal_handler_disconnect (browser, available_signal_id);
        available_signal_id = g_signal_connect (browser,
                                      "resource-available",
                                      G_CALLBACK (resource_availabe_bgo724030_2),
                                      loop);
        g_main_loop_run (loop);  /* unavailable + available */
        g_main_loop_run (loop);  /* unavailable */
        unref_object(browser);
}

/* END Regression test
 * https://bugzilla.gnome.org/show_bug.cgi?id=724030
 * ============================================================================
 */


int main (int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION (2, 35, 0)
        g_type_init ();
#endif
        g_test_init (&argc, &argv, NULL);

        if (g_test_slow ()) {
               g_test_add_func ("/bugs/gnome/673150", test_bgo673150);
               g_test_add_func ("/bugs/gnome/682099", test_bgo682099);
               g_test_add_func ("/bugs/gnome/724030", test_bgo724030);
        }

        g_test_run ();

        return 0;
}
