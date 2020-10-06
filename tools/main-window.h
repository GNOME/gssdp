// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GSSDP_DEVICE_SNIFFER_TYPE_MAIN_WINDOW                                  \
        (gssdp_device_sniffer_main_window_get_type ())

G_DECLARE_FINAL_TYPE (GSSDPDeviceSnifferMainWindow,
                      gssdp_device_sniffer_main_window,
                      GSSDP_DEVICE_SNIFFER,
                      MAIN_WINDOW,
                      GtkApplicationWindow)

G_END_DECLS
