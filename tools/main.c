/* main.c
 *
 * Copyright 2019 Jens Georg
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <libsoup/soup.h>

#include "gssdp-device-sniffer-window.h"

static void
on_activate (GtkApplication *app)
{
    GtkWindow *window;
    double w, h;
    gint window_width, window_height;

    g_type_ensure (soup_message_headers_get_type ());

    /* It's good practice to check your parameters at the beginning of the
     * function. It helps catch errors early and in development instead of
     * by your users.
     */
    g_assert (GTK_IS_APPLICATION (app));

    /* Get the current window or create one if necessary. */
    window = gtk_application_get_active_window (app);

    if (window == NULL) {
#if GTK_CHECK_VERSION(3,22,0)
        {
            gint px, py;
            GdkScreen *pointer_screen;
            GdkDisplay *display;
            GdkDevice *pointer;
            GdkMonitor *monitor;
            GdkRectangle rectangle;

            display = gdk_display_get_default ();
            pointer = gdk_seat_get_pointer (gdk_display_get_default_seat (display));

            gdk_device_get_position (pointer,
                            &pointer_screen,
                            &px, &py);

            monitor = gdk_display_get_monitor_at_point (display, px, py);


            gdk_monitor_get_geometry (monitor, &rectangle);
            w = rectangle.width * 0.75;
            h = rectangle.height * 0.75;
        }

#else
        w = gdk_screen_width () * 0.75;
        h = gdk_screen_height () * 0.75;
#endif

        window_width = CLAMP ((int) w, 10, 1000);
        window_height = CLAMP ((int) h, 10, 800);

        window = g_object_new (GSSDP_DEVICE_SNIFFER_TYPE_WINDOW,
                "application", app,
                "default-width", window_width,
                "default-height", window_height,
                NULL);
    }

    /* Ask the window manager/compositor to present the window. */
    gtk_window_present (window);
}

int
main (int   argc,
      char *argv[])
{
    GtkApplication * app = NULL;
    int ret;

#if 0
    /* Set up gettext translations */
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif
    /*
     * Create a new GtkApplication. The application manages our main loop,
     * application windows, integration with the window manager/compositor, and
     * desktop features such as file opening and single-instance applications.
     */
    app = gtk_application_new ("org.gnome.GssdpDeviceSniffer", G_APPLICATION_FLAGS_NONE);

    /*
     * We connect to the activate signal to create a window when the application
     * has been lauched. Additionally, this signal notifies us when the user
     * tries to launch a "second instance" of the application. When they try
     * to do that, we'll just present any existing window.
     *
     * Because we can't pass a pointer to any function type, we have to cast
     * our "on_activate" function to a GCallback.
     */
    g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);

    /*
     * Run the application. This function will block until the applicaiton
     * exits. Upon return, we have our exit code to return to the shell. (This
     * is the code you see when you do `echo $?` after running a command in a
     * terminal.
     *
     * Since GtkApplication inherits from GApplication, we use the parent class
     * method "run". But we need to cast, which is what the "G_APPLICATION()"
     * macro does.
     */
    ret = g_application_run (G_APPLICATION (app), argc, argv);

    g_object_unref (G_OBJECT (app));

    return ret;
}
