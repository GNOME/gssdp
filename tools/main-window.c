// SPDX-License-Identifier: LGPL-2.1-or-later

#include "main-window.h"

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
gssdp_device_sniffer_main_window_init (GSSDPDeviceSnifferMainWindow *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}
