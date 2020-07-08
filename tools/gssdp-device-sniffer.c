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

#include <libgssdp/gssdp.h>
#include <libgssdp/gssdp-client-private.h>
#include <libsoup/soup.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

#define UI_RESOURCE "/org/gupnp/GSSDP/DeviceSniffer.ui"
#define MENU_RESOURCE "/org/gupnp/GSSDP/WindowMenu.ui"

static char *interface = NULL;

typedef enum {
        PACKET_STORE_COLUMN_TIME,
        PACKET_STORE_COLUMN_IP,
        PACKET_STORE_COLUMN_INTERFACE,
        PACKET_STORE_COLUMN_PACKET_TYPE,
        PACKET_STORE_COLUMN_TARGET,
        PACKET_STORE_COLUMN_HEADERS,
        PACKET_STORE_COLUMN_RAW_ARRIVAL_TIME
} PACKET_STORE_COLUMNS;

typedef enum {
        DEVICE_STORE_COLUMN_UUID,
        DEVICE_STORE_COLUMN_FIRST_SEEN,
        DEVICE_STORE_COLUMN_TYPE,
        DEVICE_STORE_COLUMN_LOCATION
} DEVICE_STORE_COLUMNS;


GtkBuilder *builder;
GSSDPResourceBrowser *resource_browser;
GSSDPClient *client;
char *ip_filter = NULL;
gboolean capture_packets = TRUE;
gboolean prefer_v6 = FALSE;

GOptionEntry entries[] =
{
        {"interface", 'i', 0, G_OPTION_ARG_STRING, &interface, "Network interface to listen on", NULL },
        { "prefer-v6", '6', 0, G_OPTION_ARG_NONE, &prefer_v6, "Prefer IPv6 for the client", NULL },
        { "prefer-v4", '4', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &prefer_v6, "Prefer IPv4 for the client", NULL },
        { NULL }
};

void
on_enable_packet_capture_activate (GtkToggleButton  *menuitem,
                                   gpointer          user_data);

void
on_address_filter_dialog_response (GtkDialog *dialog,
                                   gint       response,
                                   gpointer   user_data);

gboolean
on_delete_event (GtkWidget *widget, GdkEvent  *event, gpointer   user_data);

G_MODULE_EXPORT void
on_enable_packet_capture_activate (GtkToggleButton *menuitem,
                                   gpointer         user_data)
{
        const gchar *icon_name = NULL;

        capture_packets = gtk_toggle_button_get_active (menuitem);
        icon_name = capture_packets
                ? "media-playback-stop-symbolic"
                : "media-playback-start-symbolic";

        gtk_image_set_from_icon_name (GTK_IMAGE (user_data),
                                      icon_name,
                                      GTK_ICON_SIZE_BUTTON);
}

static void
clear_packet_treeview (void)
{
        GtkWidget *treeview;
        GtkTreeModel *model;

        treeview = GTK_WIDGET(gtk_builder_get_object (builder, "packet-treeview"));
        g_assert (treeview != NULL);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
        gtk_list_store_clear (GTK_LIST_STORE (model));
}

static void
on_details_activate (GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data)
{
        GtkWidget *scrolled_window = NULL;

        scrolled_window = GTK_WIDGET (gtk_builder_get_object (builder,
                                                              "packet-details-scrolledwindow"));

        g_object_set (G_OBJECT (scrolled_window), "visible", g_variant_get_boolean (parameter), NULL);
        g_simple_action_set_state (action, parameter);
}

static void
packet_header_to_string (const char *header_name,
                 const char *header_val,
                 GString **text)
{
        g_string_append_printf (*text, "%s: %s\n",
                         header_name,
                         header_val);
}
 
static void
clear_textbuffer (GtkTextBuffer *textbuffer)
{
        GtkTextIter start, end;

        gtk_text_buffer_get_bounds (textbuffer, &start, &end);
        gtk_text_buffer_delete (textbuffer, &start, &end);
}

static void
update_packet_details (const char *text, unsigned int len)
{
        GtkWidget *textview;
        GtkTextBuffer *textbuffer;
        
        textview = GTK_WIDGET(gtk_builder_get_object (builder, "packet-details-textview"));
        g_assert (textview != NULL);
        textbuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
        
        clear_textbuffer (textbuffer);
        gtk_text_buffer_insert_at_cursor (textbuffer, text, len);
}

static void
display_packet (GDateTime *arrival_time, SoupMessageHeaders *packet_headers)
{
        GString *text;
        char *time = NULL;

        time = g_date_time_format_iso8601 (arrival_time);
        text = g_string_new ("Received on: ");
        g_string_append (text, time);
        g_string_append (text, "\nHeaders:\n\n");
        g_free (time);

        soup_message_headers_foreach (packet_headers,
                        (SoupMessageHeadersForeachFunc)
                        packet_header_to_string,
                        &text);

        update_packet_details (text->str, text->len);
        g_string_free (text, TRUE);
}

static void
on_packet_selected (GtkTreeSelection      *selection,
                    G_GNUC_UNUSED gpointer user_data)
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
                display_packet (arrival_time, packet_headers);
                g_boxed_free (SOUP_TYPE_MESSAGE_HEADERS, packet_headers);
                g_date_time_unref (arrival_time);
        }

        else
                update_packet_details ("", 0);
}

static const char *message_types[] = {"M-SEARCH", "RESPONSE", "NOTIFY"};

static char **
packet_to_treeview_data (const gchar        *from_ip,
                         GDateTime          *arrival_time,
                         _GSSDPMessageType   type,
                         SoupMessageHeaders *headers)
{
        char **packet_data;
        const char *target;

        packet_data = g_new0 (char *, 6);

        /* Set the Time */
        packet_data[0] = g_date_time_format (arrival_time, "%R");

        /* Now the Source Address */
        packet_data[1] = g_strdup (from_ip);
        
        packet_data[2] = g_strdup (gssdp_client_get_interface (client));

        /* Now the Packet Type */
        packet_data[3] = g_strdup (message_types[type]);
        
        /* Now the Packet Information */
        if (type == _GSSDP_DISCOVERY_RESPONSE)
                target = soup_message_headers_get_one (headers, "ST");
        else
                target = soup_message_headers_get_one (headers, "NT");
        
        packet_data[4] = g_strdup (target);
        packet_data[5] = NULL;

        return packet_data;
}

static void
append_packet (const gchar *from_ip,
               GDateTime *arrival_time,
               _GSSDPMessageType type,
               SoupMessageHeaders *headers)
{
        GtkWidget *treeview;
        GtkListStore *liststore;
        GtkTreeIter iter;
        char **packet_data;
        
        treeview = GTK_WIDGET(gtk_builder_get_object (builder, "packet-treeview"));
        g_assert (treeview != NULL);
        liststore = GTK_LIST_STORE (
                        gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));
        g_assert (liststore != NULL);
       
        packet_data = packet_to_treeview_data (from_ip,
                                               arrival_time,
                                               type,
                                               headers);

        gtk_list_store_insert_with_values (liststore, &iter, 0,
                        PACKET_STORE_COLUMN_TIME, packet_data[0],
                        PACKET_STORE_COLUMN_IP, packet_data[1],
                        PACKET_STORE_COLUMN_INTERFACE, packet_data[2],
                        PACKET_STORE_COLUMN_PACKET_TYPE, packet_data[3],
                        PACKET_STORE_COLUMN_TARGET, packet_data[4],
                        PACKET_STORE_COLUMN_HEADERS, headers,
                        PACKET_STORE_COLUMN_RAW_ARRIVAL_TIME, arrival_time,
                        -1);
        g_strfreev (packet_data);
}

static void
on_ssdp_message (G_GNUC_UNUSED GSSDPClient *ssdp_client,
                 G_GNUC_UNUSED const gchar *from_ip,
                 G_GNUC_UNUSED gushort      from_port,
                 _GSSDPMessageType          type,
                 SoupMessageHeaders        *headers,
                 G_GNUC_UNUSED gpointer     user_data)
{
        GDateTime *arrival_time;
     
        if (type == _GSSDP_DISCOVERY_REQUEST)
                return;
        if (ip_filter != NULL && strcmp (ip_filter, from_ip) != 0)
                return;
        if (!capture_packets) 
                return;

        arrival_time = g_date_time_new_now_local ();
        append_packet (from_ip, arrival_time, type, headers);
        g_date_time_unref (arrival_time);
}

static gboolean 
find_device (GtkTreeModel *model, const char *uuid, GtkTreeIter *iter)
{
        gboolean found = FALSE;
        gboolean more;
       
        more = gtk_tree_model_get_iter_first (model, iter);
        while (more) {
                char *device_uuid;
                gtk_tree_model_get (model,
                                    iter, 
                                    0, &device_uuid, -1);
                found = g_strcmp0 (device_uuid, uuid) == 0;
                g_free (device_uuid);

                if (found)
                        break;
                more = gtk_tree_model_iter_next (model, iter);
        }

        return found;
}

static void
append_device (const char *uuid,
               const char *first_notify,
               const char *device_type,
               const char *location)
{
        GtkWidget *treeview;
        GtkTreeModel *model;
        GtkTreeIter iter;
       
        treeview = GTK_WIDGET(gtk_builder_get_object (builder, "device-details-treeview"));
        g_assert (treeview != NULL);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
        g_assert (model != NULL);
       
        if (!find_device (model, uuid, &iter)) {
                gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                &iter, 0,
                                DEVICE_STORE_COLUMN_UUID, uuid,
                                DEVICE_STORE_COLUMN_FIRST_SEEN, first_notify,
                                DEVICE_STORE_COLUMN_LOCATION, location, -1);
        }
                
        if (device_type) {
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                DEVICE_STORE_COLUMN_TYPE, device_type, -1);
        }
}

static void
resource_available_cb (G_GNUC_UNUSED GSSDPResourceBrowser *ssdp_resource_browser,
                       const char                         *usn,
                       GList                              *locations)
{

        char **usn_tokens;
        char *uuid;
        char *device_type = NULL;
        GDateTime *current_time = NULL;
        char *first_notify;
                
        current_time = g_date_time_new_now_local ();
        first_notify = g_date_time_format (current_time, "%R");
        g_date_time_unref (current_time);

        usn_tokens = g_strsplit (usn, "::", -1);
        g_assert (usn_tokens != NULL && usn_tokens[0] != NULL);

        uuid = usn_tokens[0] + 5; /* skip the prefix 'uuid:' */

        if (usn_tokens[1] && strlen(usn_tokens[1]) != 0) {
                char **urn_tokens;

                urn_tokens = g_strsplit (usn_tokens[1], ":device:", -1);
                        
                if (urn_tokens[1])
                        device_type = g_strdup (urn_tokens[1]);
                g_strfreev (urn_tokens);
        }

        /* Device Announcement */
        append_device (uuid,
                       first_notify,
                       device_type,
                       (char *) locations->data);
        
        g_free (device_type);
        g_free (first_notify);
        g_strfreev (usn_tokens);
}

static void
remove_device (const char *uuid)
{
        GtkWidget *treeview;
        GtkTreeModel *model;
        GtkTreeIter iter;
       
        treeview = GTK_WIDGET(gtk_builder_get_object (builder, "device-details-treeview"));
        g_assert (treeview != NULL);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
        g_assert (model != NULL);
      
        if (find_device (model, uuid, &iter)) {
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
}

static void
resource_unavailable_cb (G_GNUC_UNUSED GSSDPResourceBrowser *ssdp_resource_browser,
                         const char                         *usn)
{
        char **usn_tokens;
        char *uuid;

        usn_tokens = g_strsplit (usn, "::", -1);
        g_assert (usn_tokens != NULL && usn_tokens[0] != NULL);
        uuid = usn_tokens[0] + 5; /* skip the prefix 'uuid:' */
        
        remove_device (uuid);
        
        g_strfreev (usn_tokens);
}

G_MODULE_EXPORT
void
on_use_filter_radiobutton_toggled (GtkToggleButton       *togglebutton,
                                   G_GNUC_UNUSED gpointer user_data)
{
        GtkWidget *filter_hbox;
        gboolean use_filter;

        filter_hbox = GTK_WIDGET(gtk_builder_get_object (builder, "address-entry0"));
        g_assert (filter_hbox != NULL);
        
        use_filter = gtk_toggle_button_get_active (togglebutton);
        gtk_widget_set_sensitive (filter_hbox, use_filter);
}

static char *
get_ip_filter (void)
{
        GtkEntry *entry;
        GInetAddress *addr;

        entry = GTK_ENTRY (gtk_builder_get_object (builder, "address-entry0"));
        addr = g_inet_address_new_from_string (gtk_entry_get_text (entry));

        if (addr == NULL) {
                g_warning ("Filter not a valid IP address");

                return NULL;
        }
        g_object_unref (addr);

        return g_strdup (gtk_entry_get_text (entry));
}

G_MODULE_EXPORT
void
on_address_filter_dialog_response (GtkDialog             *dialog,
                                   G_GNUC_UNUSED gint     response,
                                   G_GNUC_UNUSED gpointer user_data)
{
        GtkWidget *use_filter_radio;

        gtk_widget_hide (GTK_WIDGET (dialog));
        
        use_filter_radio = GTK_WIDGET(gtk_builder_get_object (builder, "use-filter-radiobutton"));
        g_assert (use_filter_radio != NULL);
        
        if (response != GTK_RESPONSE_OK)
                return;
        
        g_free (ip_filter);
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (use_filter_radio))) {
                ip_filter = get_ip_filter ();
        }
       
        else
                ip_filter = NULL;
}

static gboolean
on_treeview_popup_menu (GtkWidget *tv, GdkEventButton *event, gpointer user_data)
{
        if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
                gtk_menu_popup_at_pointer (GTK_MENU (user_data), (GdkEvent *)event);
        }

        return FALSE;
}

static void
setup_treeviews (void)
{
        GtkWidget *treeview;
        GtkTreeSelection *selection;
        GtkWidget *menu = NULL;

        treeview = GTK_WIDGET(gtk_builder_get_object (builder,
                                                      "packet-treeview"));
        
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        g_signal_connect (selection,
                          "changed",
                          G_CALLBACK (on_packet_selected),
                          (gpointer *) treeview);
        menu = gtk_menu_new_from_model (G_MENU_MODEL (gtk_builder_get_object (builder,
                                                                              "sniffer-context-menu")));
        gtk_menu_attach_to_widget (GTK_MENU (menu), treeview, NULL);
        g_signal_connect (G_OBJECT (treeview),
                          "button-press-event",
                          G_CALLBACK (on_treeview_popup_menu),
                          menu);
}

G_MODULE_EXPORT
gboolean
on_delete_event (G_GNUC_UNUSED GtkWidget *widget,
                 G_GNUC_UNUSED GdkEvent  *event,
                 G_GNUC_UNUSED gpointer   user_data)
{
        gtk_main_quit ();

        return TRUE;
}

static void
on_show_address_filter (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
        GtkWidget *dialog = NULL;

        dialog = GTK_WIDGET (gtk_builder_get_object (builder,
                                                     "address-filter-dialog"));
        gtk_widget_show (dialog);
}

static void
on_set_address_filter (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
        GtkWidget *treeview = NULL;
        GtkTreeSelection *selection = NULL;
        GtkTreeModel *model = NULL;
        GtkTreeIter iter;
        GtkWidget *use_filter_radio;

        treeview = GTK_WIDGET (gtk_builder_get_object (builder, "packet-treeview"));
        use_filter_radio = GTK_WIDGET(gtk_builder_get_object (builder, "use-filter-radiobutton"));

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        gtk_tree_selection_get_selected (selection, &model, &iter);
        g_free (ip_filter);
        gtk_tree_model_get (model, &iter,
                        PACKET_STORE_COLUMN_IP, &ip_filter, -1);
        gtk_entry_set_text (GTK_ENTRY (gtk_builder_get_object (builder, "address-entry0")),
                            ip_filter);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_filter_radio), TRUE);
}

static void
on_about (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
        GtkWidget *dialog = NULL;

        dialog = GTK_WIDGET (gtk_builder_get_object (builder,
                                                     "about-dialog"));
        gtk_widget_show (dialog);
}


G_MODULE_EXPORT
void
on_clear_packet_capture_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                  G_GNUC_UNUSED gpointer     user_data)
{
        clear_packet_treeview ();
}

static void
on_trigger_rescan (GSimpleAction *action,
                   GVariant *parameter,
                   gpointer user_data)
{
        gssdp_resource_browser_rescan (resource_browser);
}

static GActionEntry actions[] = {
        { "trigger-rescan", on_trigger_rescan },
        { "set-address-filter", on_set_address_filter },
        { "show-packet-details", NULL, NULL, "true", on_details_activate },
        { "show-address-filter", on_show_address_filter },
        { "about", on_about }
};

static gboolean
init_ui (gint *argc, gchar **argv[])
{
        GtkWidget *main_window;
        gint window_width, window_height;
        GError *error = NULL;
        GOptionContext *context;
        double w, h;
        GSimpleActionGroup *group = NULL;

        context = g_option_context_new ("- graphical SSDP debug tool");
        g_option_context_add_main_entries (context, entries, NULL);
        g_option_context_add_group (context, gtk_get_option_group (TRUE));
        if (!g_option_context_parse (context, argc, argv, &error)) {
                g_print ("Failed to parse options: %s\n", error->message);
                g_error_free (error);

                return FALSE;
        }

        builder = gtk_builder_new();
        if (gtk_builder_add_from_resource(builder, UI_RESOURCE, NULL) == 0)
                return FALSE;

        if (gtk_builder_add_from_resource (builder, MENU_RESOURCE, NULL) == 0)
                return FALSE;

        gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (gtk_builder_get_object (builder, "window-menu")),
                        G_MENU_MODEL (gtk_builder_get_object (builder, "sniffer-window-menu")));

        main_window = GTK_WIDGET(gtk_builder_get_object (builder, "main-window"));
        g_assert (main_window != NULL);

        group = g_simple_action_group_new ();
        gtk_widget_insert_action_group (GTK_WIDGET (main_window), "win", G_ACTION_GROUP (group));
        g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), NULL);


#if GTK_CHECK_VERSION(3,22,0)
        gtk_widget_realize (main_window);
        {
            GdkWindow *window = gtk_widget_get_window (main_window);
            GdkDisplay *display = gdk_display_get_default ();
            GdkMonitor *monitor = gdk_display_get_monitor_at_window (display,
                                                                     window);
            GdkRectangle rectangle;

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
        gtk_window_set_default_size (GTK_WINDOW (main_window),
                                     window_width,
                                     window_height);

        gtk_builder_connect_signals (builder, NULL);
        setup_treeviews ();
        gtk_widget_show_all (main_window);

        return TRUE;
}

static void
deinit_ui (void)
{
        g_object_unref (builder);
}

static gboolean
init_upnp (void)
{
        GError *error;
        
        error = NULL;
        client = g_initable_new (GSSDP_TYPE_CLIENT,
                                 NULL,
                                 &error,
                                 "address-family", prefer_v6 ? G_SOCKET_FAMILY_IPV6 : G_SOCKET_FAMILY_IPV4,
                                 "interface", interface,
                                 NULL);
        if (error) {
                g_printerr ("Error creating the GSSDP client: %s\n",
                            error->message);

                g_error_free (error);

                return FALSE;
        }

        resource_browser = gssdp_resource_browser_new (client,
                                                       GSSDP_ALL_RESOURCES);
        
        g_signal_connect (client,
                          "message-received",
                          G_CALLBACK (on_ssdp_message),
                          NULL);
        g_signal_connect (resource_browser,
                          "resource-available",
                          G_CALLBACK (resource_available_cb),
                          NULL);
        g_signal_connect (resource_browser,
                          "resource-unavailable",
                          G_CALLBACK (resource_unavailable_cb),
                          NULL);

        gssdp_resource_browser_set_active (resource_browser, TRUE);

        return TRUE;
}

static void
deinit_upnp (void)
{
        g_object_unref (resource_browser);
        g_object_unref (client);
}

gint
main (gint argc, gchar *argv[])
{
        g_type_ensure (G_TYPE_DATE_TIME);

        if (!init_ui (&argc, &argv)) {
           return -2;
        }

        if (!init_upnp ()) {
           return -3;
        }
        
        gtk_main ();
       
        deinit_upnp ();
        deinit_ui ();
        
        return 0;
}
