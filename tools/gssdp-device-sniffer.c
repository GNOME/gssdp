/* 
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
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

#include "main-window.h"

#include <libgssdp/gssdp.h>
#include <libgssdp/gssdp-client-private.h>
#include <libsoup/soup.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>


static void
on_activate (GtkApplication *app)
{
        GtkWindow *window;

        window = gtk_application_get_active_window (app);
        if (window == NULL) {
                window = g_object_new (GSSDP_DEVICE_SNIFFER_TYPE_MAIN_WINDOW,
                                       "application",
                                       app,
                                       NULL);
        }

        gtk_window_present (window);
}

static int
on_command_line (GtkApplication *app,
                 GApplicationCommandLine *cmdline,
                 gpointer user_data)
{
        char *iface = NULL;
        GSocketFamily family = G_SOCKET_FAMILY_INVALID;
        gboolean six = FALSE;


        GVariantDict *args = g_application_command_line_get_options_dict (cmdline);

        GOptionContext *context = g_option_context_new (NULL);
        g_option_context_set_help_enabled (context, FALSE);

        g_variant_dict_lookup (args, "interface" ,"s", &iface);
        g_variant_dict_lookup (args, "prefer-v6", "b", &six);


        if (six) {
                family = G_SOCKET_FAMILY_IPV6;
        } else {
                family = G_SOCKET_FAMILY_IPV4;
        }

        GtkWindow *window;
        window = g_object_new (GSSDP_DEVICE_SNIFFER_TYPE_MAIN_WINDOW,
                               "application",
                               app,
                               "address-family",
                               family,
                               "interface",
                               iface,
                               NULL);

        gtk_window_present (window);

        return EXIT_SUCCESS;
}

GOptionEntry entries[] = { { "interface",
                             'i',
                             0,
                             G_OPTION_ARG_STRING,
                             NULL,
                             "Network interface to listen on",
                             NULL },
                           { "prefer-v6",
                             '6',
                             0,
                             G_OPTION_ARG_NONE,
                             NULL,
                             "Prefer IPv6 for the client",
                             NULL },
                           { NULL , 0, 0, 0, NULL, NULL, NULL} };

gint
main (gint argc, gchar *argv[])
{
        g_type_ensure (G_TYPE_DATE_TIME);

        GtkApplication *app =
                gtk_application_new ("org.gupnp.GSSDP.DeviceSniffer",
                                     G_APPLICATION_HANDLES_COMMAND_LINE);

        g_application_add_main_option_entries (G_APPLICATION (app), entries);
        g_application_set_option_context_parameter_string (
                G_APPLICATION (app),
                "- graphical SSDP debug tool");

        g_signal_connect (G_OBJECT (app),
                          "command-line",
                          G_CALLBACK (on_command_line),
                          NULL);
        g_signal_connect (G_OBJECT (app),
                          "activate",
                          G_CALLBACK (on_activate),
                          NULL);

        return g_application_run (G_APPLICATION (app), argc, argv);
}
