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
        GSSDPClient *client = NULL;
        GError *error = NULL;
        GInetAddress *lo =
                g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);

        client = gssdp_client_new_for_address (lo,
                                               0,
                                               GSSDP_UDA_VERSION_1_0,
                                               &error);

        g_assert_no_error (error);
        g_assert_nonnull (client);
        g_clear_object (&lo);

        return client;
}
