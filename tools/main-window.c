// SPDX-License-Identifier: LGPL-2.1-or-later

#include "main-window.h"

#include <glib.h>

struct _GSSDPDeviceSnifferMainWindow {
        GtkApplicationWindow parent_instance;
};

G_DEFINE_TYPE (GSSDPDeviceSnifferMainWindow,
               gssdp_device_sniffer_main_window,
               GTK_TYPE_APPLICATION_WINDOW)

static void
gssdp_device_sniffer_main_window_class_init (
        GSSDPDeviceSnifferMainWindowClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (
                widget_class,
                "/org/gupnp/GSSDP/MainWindow.ui");
}

static void
on_realize (GtkWidget *self, gpointer user_data)
{
        double w;
        double h;
#if GTK_CHECK_VERSION(3, 22, 0)
        {
                GdkWindow *window = gtk_widget_get_window (self);
                GdkDisplay *display = gdk_display_get_default ();
                GdkMonitor *monitor =
                        gdk_display_get_monitor_at_window (display, window);
                GdkRectangle rectangle;

                gdk_monitor_get_geometry (monitor, &rectangle);
                w = rectangle.width * 0.75;
                h = rectangle.height * 0.75;
        }

#else
        w = gdk_screen_width () * 0.75;
        h = gdk_screen_height () * 0.75;
#endif

        int window_width = CLAMP ((int) w, 10, 1000);
        int window_height = CLAMP ((int) h, 10, 800);
        gtk_window_set_default_size (GTK_WINDOW (self),
                                     window_width,
                                     window_height);
}

static void
gssdp_device_sniffer_main_window_init (GSSDPDeviceSnifferMainWindow *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        g_signal_connect (G_OBJECT (self),
                          "realize",
                          G_CALLBACK (on_realize),
                          NULL);
}
