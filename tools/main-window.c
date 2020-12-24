// SPDX-License-Identifier: LGPL-2.1-or-later

#include "main-window.h"

#include <gio/gio.h>
#include <glib.h>

#include <libsoup/soup.h>

typedef enum
{
        PACKET_STORE_COLUMN_TIME,
        PACKET_STORE_COLUMN_IP,
        PACKET_STORE_COLUMN_INTERFACE,
        PACKET_STORE_COLUMN_PACKET_TYPE,
        PACKET_STORE_COLUMN_TARGET,
        PACKET_STORE_COLUMN_HEADERS,
        PACKET_STORE_COLUMN_RAW_ARRIVAL_TIME
} PACKET_STORE_COLUMNS;

enum
{
        PROP_0,
        PROP_INTERFACE,
        PROP_ADDRESS_FAMILY,
};

struct _GSSDPDeviceSnifferMainWindow {
        GtkApplicationWindow parent_instance;

        // SSDP related parameters
        char *interface;
        GSocketFamily family;

        // Bound child widgets
        GtkWidget *packet_treeview;
        GtkWidget *packet_textview;
};

G_DEFINE_TYPE (GSSDPDeviceSnifferMainWindow,
               gssdp_device_sniffer_main_window,
               GTK_TYPE_APPLICATION_WINDOW)

static void
main_window_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
        switch (property_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
main_window_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
        GSSDPDeviceSnifferMainWindow *self =
                GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (object);

        switch (property_id) {
        case PROP_INTERFACE:
                self->interface = g_value_dup_string (value);
                break;
        case PROP_ADDRESS_FAMILY:
                self->family = g_value_get_enum (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
gssdp_device_sniffer_main_window_class_init (
        GSSDPDeviceSnifferMainWindowClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (
                widget_class,
                "/org/gupnp/GSSDP/MainWindow.ui");
        gtk_widget_class_bind_template_child (widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              packet_treeview);
        gtk_widget_class_bind_template_child (widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              packet_textview);

        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->set_property = main_window_set_property;
        object_class->get_property = main_window_get_property;

        g_object_class_install_property (
                object_class,
                PROP_ADDRESS_FAMILY,
                g_param_spec_enum (
                        "address-family",
                        "Socket address familiy",
                        "The socket address familiy of the SSDP client",
                        G_TYPE_SOCKET_FAMILY,
                        G_SOCKET_FAMILY_INVALID,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                                G_PARAM_STATIC_STRINGS));

        g_object_class_install_property (
                object_class,
                PROP_INTERFACE,
                g_param_spec_string ("interface",
                                     "Network interface",
                                     "The network interface of the SSDP client",
                                     NULL,
                                     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                                             G_PARAM_STATIC_STRINGS));
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
packet_header_to_string (const char *header_name,
                         const char *header_val,
                         GString **text)
{
        g_string_append_printf (*text, "%s: %s\n", header_name, header_val);
}

static void
clear_textbuffer (GtkTextBuffer *textbuffer)
{
        GtkTextIter start, end;

        gtk_text_buffer_get_bounds (textbuffer, &start, &end);
        gtk_text_buffer_delete (textbuffer, &start, &end);
}

static void
update_packet_details (GSSDPDeviceSnifferMainWindow *self,
                       const char *text,
                       int len)
{
        GtkTextBuffer *textbuffer;

        textbuffer = gtk_text_view_get_buffer (
                GTK_TEXT_VIEW (self->packet_textview));

        clear_textbuffer (textbuffer);
        gtk_text_buffer_insert_at_cursor (textbuffer, text, len);
}

static void
display_packet (GSSDPDeviceSnifferMainWindow *self,
                GDateTime *arrival_time,
                SoupMessageHeaders *packet_headers)
{
        GString *text;
        char *time = NULL;

        time = g_date_time_format_iso8601 (arrival_time);
        text = g_string_new ("Received on: ");
        g_string_append (text, time);
        g_string_append (text, "\nHeaders:\n\n");
        g_free (time);

        soup_message_headers_foreach (
                packet_headers,
                (SoupMessageHeadersForeachFunc) packet_header_to_string,
                &text);

        update_packet_details (self, text->str, text->len);
        g_string_free (text, TRUE);
}

static void
on_packet_selected (GtkTreeSelection *selection, gpointer user_data)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        GDateTime *arrival_time;

        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                SoupMessageHeaders *packet_headers;

                gtk_tree_model_get (model,
                                    &iter,
                                    PACKET_STORE_COLUMN_HEADERS,
                                    &packet_headers,
                                    PACKET_STORE_COLUMN_RAW_ARRIVAL_TIME,
                                    &arrival_time,
                                    -1);
                display_packet (GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (user_data),
                                arrival_time,
                                packet_headers);
                g_boxed_free (SOUP_TYPE_MESSAGE_HEADERS, packet_headers);
                g_date_time_unref (arrival_time);
        }

        else
                update_packet_details (
                        GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (user_data),
                        "",
                        0);
}

static void
gssdp_device_sniffer_main_window_init (GSSDPDeviceSnifferMainWindow *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        g_signal_connect (G_OBJECT (self),
                          "realize",
                          G_CALLBACK (on_realize),
                          NULL);

        GtkTreeSelection *selection = gtk_tree_view_get_selection (
                GTK_TREE_VIEW (self->packet_treeview));
        g_signal_connect (G_OBJECT (selection),
                          "changed",
                          G_CALLBACK (on_packet_selected),
                          self);
}
