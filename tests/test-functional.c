/*
 * Copyright (C) 2012 Openismus GmbH
 *
 * Author: Jens Georg <jensg@openismus.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <string.h>

#include <gio/gio.h>

#include <libgssdp/gssdp-resource-browser.h>
#include <libgssdp/gssdp-protocol.h>

#include "test-util.h"

#define UUID_1 "uuid:81909e94-ebf4-469e-ac68-81f2f189de1b"
#define VERSIONED_NT_1 "urn:org-gupnp:device:FunctionalTest:1"
#define VERSIONED_NT_2 "urn:org-gupnp:device:FunctionalTest:9"
#define VERSIONED_USN_1 UUID_1"::"VERSIONED_NT_1
#define VERSIONED_USN_2 UUID_1"::"VERSIONED_NT_2

/* Helper functions */

static GSocket *
create_socket (void)
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
create_alive_message (const char *nt)
{
        char *usn, *msg;

        if (strcmp (nt, UUID_1) == 0)
                usn = g_strdup (UUID_1);
        else
                usn = g_strconcat (UUID_1, "::", nt, NULL);

        msg = g_strdup_printf (SSDP_ALIVE_MESSAGE "\r\n",
                               SSDP_ADDR,
                               1800,
                               "http://127.0.0.1:1234",
                               "",
                               "Linux/3.0 UPnP/1.0 GSSDPTesting/0.0.0",
                               nt,
                               usn);
        g_free (usn);

        return msg;
}

static char *
create_byebye_message (const char *nt)
{
        char *usn, *msg;

        if (strcmp (nt, UUID_1) == 0)
                usn = g_strdup (UUID_1);
        else
                usn = g_strconcat (UUID_1, "::", nt, NULL);

        msg = g_strdup_printf (SSDP_BYEBYE_MESSAGE "\r\n", SSDP_ADDR, nt, usn);
        g_free (usn);

        return msg;
}

typedef struct {
        const char *usn;
        GMainLoop  *loop;
        gboolean    found;
} TestDiscoverySSDPAllData;


static void
on_test_discovery_ssdp_all_resource_available (GSSDPResourceBrowser *src,
                                               const char           *usn,
                                               gpointer              locations,
                                               gpointer              user_data)
{
        TestDiscoverySSDPAllData *data = (TestDiscoverySSDPAllData *) user_data;

        g_assert_cmpstr (usn, ==, data->usn);

        data->found = TRUE;

        g_main_loop_quit (data->loop);
}

static void
on_test_discovery_ssdp_all_resource_unavailable (GSSDPResourceBrowser *src,
                                                 const char           *usn,
                                                 gpointer              user_data)
{
        TestDiscoverySSDPAllData *data = (TestDiscoverySSDPAllData *) user_data;

        g_assert_cmpstr (usn, ==, data->usn);

        data->found = TRUE;

        g_main_loop_quit (data->loop);
}

static gboolean
test_discovery_send_packet (gpointer user_data)
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

static void
test_discovery_ssdp_all (void)
{
        GSSDPClient *client;
        GSSDPResourceBrowser *browser;
        GError *error = NULL;
        TestDiscoverySSDPAllData data;
        gulong signal_id;
        guint timeout_id;

        data.loop = g_main_loop_new (NULL, FALSE);
        data.usn = UUID_1"::MyService:1";
        data.found = FALSE;

        client = get_client (&error);
        g_assert (client != NULL);
        g_assert (error == NULL);

        browser = gssdp_resource_browser_new (client, "ssdp:all");
        signal_id = g_signal_connect (browser,
                                      "resource-available",
                                      G_CALLBACK (on_test_discovery_ssdp_all_resource_available),
                                      &data);
        gssdp_resource_browser_set_active (browser, TRUE);

        timeout_id = g_timeout_add_seconds (10, quit_loop, data.loop);

        /* delay announcement until browser issued its M-SEARCH */
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_alive_message ("MyService:1"));
        g_main_loop_run (data.loop);

        g_assert (data.found);

        data.found = FALSE;
        g_signal_handler_disconnect (browser, signal_id);
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_byebye_message ("MyService:1"));
        signal_id = g_signal_connect (browser,
                                      "resource-unavailable",
                                      G_CALLBACK (on_test_discovery_ssdp_all_resource_unavailable),
                                      &data);
        g_main_loop_run (data.loop);
        g_signal_handler_disconnect (browser, signal_id);

        g_assert (data.found);

        g_source_remove (timeout_id);
        g_object_unref (browser);
        g_object_unref (client);
        g_main_loop_unref (data.loop);
}

static void
test_discovery_upnp_rootdevice (void)
{
        GSSDPClient *client;
        GSSDPResourceBrowser *browser;
        GError *error = NULL;
        TestDiscoverySSDPAllData data;
        gulong signal_id;
        guint timeout_id;

        data.loop = g_main_loop_new (NULL, FALSE);
        data.usn = UUID_1"::upnp:rootdevice";
        data.found = FALSE;

        client = get_client (&error);
        g_assert (client != NULL);
        g_assert (error == NULL);

        browser = gssdp_resource_browser_new (client, "upnp:rootdevice");
        signal_id = g_signal_connect (browser,
                                      "resource-available",
                                      G_CALLBACK (on_test_discovery_ssdp_all_resource_available),
                                      &data);
        gssdp_resource_browser_set_active (browser, TRUE);

        timeout_id = g_timeout_add_seconds (10, quit_loop, data.loop);

        /* delay announcement until browser issued its M-SEARCH */
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_alive_message ("upnp:rootdevice"));
        g_main_loop_run (data.loop);

        g_assert (data.found);

        data.found = FALSE;
        g_signal_handler_disconnect (browser, signal_id);
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_byebye_message ("upnp:rootdevice"));
        signal_id = g_signal_connect (browser,
                                      "resource-unavailable",
                                      G_CALLBACK (on_test_discovery_ssdp_all_resource_unavailable),
                                      &data);
        g_main_loop_run (data.loop);
        g_signal_handler_disconnect (browser, signal_id);

        g_assert (data.found);

        g_source_remove (timeout_id);
        g_object_unref (browser);
        g_object_unref (client);
        g_main_loop_unref (data.loop);
}

static void
test_discovery_uuid (void)
{
        GSSDPClient *client;
        GSSDPResourceBrowser *browser;
        GError *error = NULL;
        TestDiscoverySSDPAllData data;
        gulong signal_id;
        guint timeout_id;

        data.loop = g_main_loop_new (NULL, FALSE);
        data.usn = UUID_1;
        data.found = FALSE;

        client = get_client (&error);
        g_assert (client != NULL);
        g_assert (error == NULL);

        browser = gssdp_resource_browser_new (client, UUID_1);
        signal_id = g_signal_connect (browser,
                                      "resource-available",
                                      G_CALLBACK (on_test_discovery_ssdp_all_resource_available),
                                      &data);
        gssdp_resource_browser_set_active (browser, TRUE);

        timeout_id = g_timeout_add_seconds (10, quit_loop, data.loop);

        /* delay announcement until browser issued its M-SEARCH */
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_alive_message (UUID_1));
        g_main_loop_run (data.loop);

        g_assert (data.found);

        data.found = FALSE;
        g_signal_handler_disconnect (browser, signal_id);
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_byebye_message (UUID_1));
        signal_id = g_signal_connect (browser,
                                      "resource-unavailable",
                                      G_CALLBACK (on_test_discovery_ssdp_all_resource_unavailable),
                                      &data);
        g_main_loop_run (data.loop);
        g_signal_handler_disconnect (browser, signal_id);

        g_assert (data.found);

        g_source_remove (timeout_id);
        g_object_unref (browser);
        g_object_unref (client);
        g_main_loop_unref (data.loop);
}


static void
test_discovery_versioned (void)
{
        GSSDPClient *client;
        GSSDPResourceBrowser *browser;
        GError *error = NULL;
        TestDiscoverySSDPAllData data;
        gulong signal_id;
        guint timeout_id;

        data.loop = g_main_loop_new (NULL, FALSE);
        data.usn = VERSIONED_USN_1;
        data.found = FALSE;

        client = get_client (&error);
        g_assert (client != NULL);
        g_assert (error == NULL);

        browser = gssdp_resource_browser_new (client, VERSIONED_NT_1);
        signal_id = g_signal_connect (browser,
                                      "resource-available",
                                      G_CALLBACK (on_test_discovery_ssdp_all_resource_available),
                                      &data);
        gssdp_resource_browser_set_active (browser, TRUE);

        timeout_id = g_timeout_add_seconds (10, quit_loop, data.loop);

        /* delay announcement until browser issued its M-SEARCH */
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_alive_message (VERSIONED_NT_1));
        g_main_loop_run (data.loop);

        g_assert (data.found);

        data.found = FALSE;
        g_signal_handler_disconnect (browser, signal_id);
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_byebye_message (VERSIONED_NT_1));
        signal_id = g_signal_connect (browser,
                                      "resource-unavailable",
                                      G_CALLBACK (on_test_discovery_ssdp_all_resource_unavailable),
                                      &data);
        g_main_loop_run (data.loop);

        g_assert (data.found);

        /* check that the resource group doesn't trigger on other resources */
        g_signal_handler_disconnect (browser, signal_id);
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_byebye_message ("MyService:1"));
        g_signal_connect (browser,
                          "resource-available",
                          G_CALLBACK (on_resource_available_assert_not_reached),
                          NULL);
        g_source_remove (timeout_id);
        g_timeout_add_seconds (5, quit_loop, data.loop);
        g_main_loop_run (data.loop);

        g_object_unref (browser);
        g_object_unref (client);
        g_main_loop_unref (data.loop);
}

/*
 * Search for FunctionalTest:1 and accept FunctionalTest:9
 */
static void
test_discovery_versioned_backwards_compatible (void)
{
        GSSDPClient *client;
        GSSDPResourceBrowser *browser;
        GError *error = NULL;
        TestDiscoverySSDPAllData data;
        gulong signal_id;
        guint timeout_id;

        data.loop = g_main_loop_new (NULL, FALSE);
        data.usn = VERSIONED_USN_2;
        data.found = FALSE;

        client = get_client (&error);
        g_assert (client != NULL);
        g_assert (error == NULL);

        browser = gssdp_resource_browser_new (client, VERSIONED_NT_1);
        signal_id = g_signal_connect (browser,
                                      "resource-available",
                                      G_CALLBACK (on_test_discovery_ssdp_all_resource_available),
                                      &data);
        gssdp_resource_browser_set_active (browser, TRUE);

        timeout_id = g_timeout_add_seconds (10, quit_loop, data.loop);

        /* delay announcement until browser issued its M-SEARCH */
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_alive_message (VERSIONED_NT_2));
        g_main_loop_run (data.loop);

        g_assert (data.found);

        data.found = FALSE;
        g_signal_handler_disconnect (browser, signal_id);
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_byebye_message (VERSIONED_NT_2));
        g_signal_connect (
                browser,
                "resource-unavailable",
                G_CALLBACK (on_test_discovery_ssdp_all_resource_unavailable),
                &data);
        g_main_loop_run (data.loop);

        g_assert (data.found);

        g_source_remove (timeout_id);
        g_object_unref (browser);
        g_object_unref (client);
        g_main_loop_unref (data.loop);
}

/*
 * Search for FunctionalTest:9 and ignore FunctionalTest:1
 */
static void
test_discovery_versioned_ignore_older (void)
{
        GSSDPClient *client;
        GSSDPResourceBrowser *browser;
        GError *error = NULL;
        GMainLoop *loop;

        loop = g_main_loop_new (NULL, FALSE);

        client = get_client (&error);
        g_assert (client != NULL);
        g_assert (error == NULL);

        browser = gssdp_resource_browser_new (client, VERSIONED_NT_2);
        g_signal_connect (browser,
                          "resource-available",
                          G_CALLBACK (on_resource_available_assert_not_reached),
                          NULL);
        gssdp_resource_browser_set_active (browser, TRUE);

        g_timeout_add_seconds (5, quit_loop, loop);

        /* delay announcement until browser issued its M-SEARCH */
        g_timeout_add_seconds (1,
                               test_discovery_send_packet,
                               create_alive_message (VERSIONED_NT_1));
        g_main_loop_run (loop);

        g_object_unref (browser);
        g_object_unref (client);
        g_main_loop_unref (loop);
}

void
test_client_creation ()
{
        GError *error = NULL;

        GSSDPClient *client = NULL;
        client = gssdp_client_new_for_address (NULL,
                                               0,
                                               GSSDP_UDA_VERSION_1_0,
                                               &error);
        g_assert_no_error (error);
        g_assert_nonnull (client);

        g_clear_object (&client);

        GInetAddress *addr = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
        client = gssdp_client_new_for_address (addr,
                                               0,
                                               GSSDP_UDA_VERSION_1_0,
                                               &error);
        g_assert_no_error (error);
        g_assert_nonnull (client);
        g_assert_cmpint (G_SOCKET_FAMILY_IPV4,
                         ==,
                         gssdp_client_get_family (client));
        g_clear_object (&client);
        g_clear_object (&addr);

        addr = g_inet_address_new_any (G_SOCKET_FAMILY_IPV6);
        client = gssdp_client_new_for_address (addr,
                                               0,
                                               GSSDP_UDA_VERSION_1_0,
                                               &error);
        g_assert_no_error (error);
        g_assert_nonnull (client);
        g_assert_cmpint (G_SOCKET_FAMILY_IPV6,
                         ==,
                         gssdp_client_get_family (client));
        g_clear_object (&client);
        g_clear_object (&addr);
}

int main(int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/functional/resource-group/discovery/ssdp:all",
                         test_discovery_ssdp_all);

        g_test_add_func ("/functional/resource-group/discovery/upnp:rootdevice",
                         test_discovery_upnp_rootdevice);

        g_test_add_func ("/functional/resource-group/discovery/uuid",
                         test_discovery_uuid);

        g_test_add_func ("/functional/resource-group/discovery/versioned/matching",
                         test_discovery_versioned);

        g_test_add_func ("/functional/resource-group/discovery/versioned/backwards-compatible",
                         test_discovery_versioned_backwards_compatible);

        g_test_add_func ("/functional/resource-group/discovery/versioned/ignore-older",
                         test_discovery_versioned_ignore_older);

        g_test_add_func ("/functional/creation", test_client_creation);

        g_test_run ();

        return 0;
}
