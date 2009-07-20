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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <libgssdp/gssdp.h>
#include <libgssdp/gssdp-client-private.h>
#include <libsoup/soup.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

#define UI_FILE "gssdp-device-sniffer.ui"
#define MAX_IP_LEN 16

GtkBuilder *builder;
GSSDPResourceBrowser *resource_browser;
GSSDPClient *client;
char *ip_filter = NULL;
gboolean capture_packets = TRUE;

G_MODULE_EXPORT
void
on_enable_packet_capture_activate (GtkCheckMenuItem *menuitem, gpointer user_data)
{
        capture_packets = gtk_check_menu_item_get_active (menuitem);
}

static void
clear_packet_treeview ()
{
        GtkWidget *treeview;
        GtkTreeModel *model;
        GtkTreeIter iter;
        gboolean more;
        time_t *arrival_time;

        treeview = GTK_WIDGET(gtk_builder_get_object (builder, "packet-treeview"));
        g_assert (treeview != NULL);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
        more = gtk_tree_model_get_iter_first (model, &iter);

        while (more) {
                gtk_tree_model_get (model,
                                &iter, 
                                5, &arrival_time, -1);
                g_free (arrival_time);
                more = gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
}

G_MODULE_EXPORT
void
on_details_activate (GtkWidget *scrolled_window, GtkCheckMenuItem *menuitem)
{
        gboolean active;

        active = gtk_check_menu_item_get_active (menuitem);
        g_object_set (G_OBJECT (scrolled_window), "visible", active, NULL);
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
update_packet_details (char *text, unsigned int len)
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
display_packet (time_t arrival_time, SoupMessageHeaders *packet_headers)
{
        GString *text;

        text = g_string_new ("");
        g_string_printf (text, "Received on: %s\nHeaders:\n\n",
                        ctime (&arrival_time));

        soup_message_headers_foreach (packet_headers,
                        (SoupMessageHeadersForeachFunc)
                        packet_header_to_string,
                        &text);

        update_packet_details (text->str, text->len);
        g_string_free (text, TRUE);
}

static void
on_packet_selected (GtkTreeSelection *selection, gpointer user_data)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        time_t *arrival_time;

        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                SoupMessageHeaders *packet_headers;

                gtk_tree_model_get (model,
                                &iter, 
                                4, &packet_headers,
                                5, &arrival_time, -1);
                display_packet (*arrival_time, packet_headers);
                g_boxed_free (SOUP_TYPE_MESSAGE_HEADERS, packet_headers);
        }

        else
                update_packet_details ("", 0);
}

G_MODULE_EXPORT
void
on_clear_packet_capture_activate (GtkMenuItem *menuitem, gpointer user_data)
{
        clear_packet_treeview ();
}

static char *message_types[] = {"M-SEARCH", "RESPONSE", "NOTIFY"};

static char **
packet_to_treeview_data (const gchar *from_ip,
                time_t arrival_time,
                _GSSDPMessageType type,
                SoupMessageHeaders *headers)
{
        char **packet_data;
        const char *target;
        struct tm *tm;

        packet_data = g_malloc (sizeof (char *) * 5);

        /* Set the Time */
        tm = localtime (&arrival_time);
        packet_data[0] = g_strdup_printf ("%02d:%02d", tm->tm_hour, tm->tm_min);

        /* Now the Source Address */
        packet_data[1] = g_strdup (from_ip);
        
        /* Now the Packet Type */
        packet_data[2] = g_strdup (message_types[type]);
        
        /* Now the Packet Information */
        if (type == _GSSDP_DISCOVERY_RESPONSE)
                target = soup_message_headers_get (headers, "ST");
        else
                target = soup_message_headers_get (headers, "NT");
        
        packet_data[3] = g_strdup (target);
        packet_data[4] = NULL;

        return packet_data;
}

static void
append_packet (const gchar *from_ip,
               time_t arrival_time,
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
                        0, packet_data[0],
                        1, packet_data[1],
                        2, packet_data[2],
                        3, packet_data[3],
                        4, headers,
                        5, g_memdup (&arrival_time, sizeof (time_t)),
                        -1);
        g_strfreev (packet_data);
}

static void
on_ssdp_message (GSSDPClient *client,
                const gchar *from_ip,
                gushort from_port,
                _GSSDPMessageType type,
                SoupMessageHeaders *headers,
                gpointer user_data)
{
        time_t arrival_time;
        
        arrival_time = time (NULL);
     
        if (type == _GSSDP_DISCOVERY_REQUEST)
                return;
        if (ip_filter != NULL && strcmp (ip_filter, from_ip) != 0)
                return;
        if (!capture_packets) 
                return;

        append_packet (from_ip, arrival_time, type, headers);
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
                if (device_uuid && strcmp (device_uuid, uuid) == 0) {
                        found = TRUE;
                        break;
                }

                g_free (device_uuid);
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
                                0, uuid,
                                1, first_notify,
                                3, location, -1);
        }
                
        if (device_type) {
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                2, device_type, -1);
        }
}

static void
resource_available_cb (GSSDPResourceBrowser *resource_browser,
                       const char           *usn,
                       GList                *locations)
{

        char **usn_tokens;
        char *uuid;
        char *device_type = NULL;
        time_t current_time;
        struct tm *tm;
        char *first_notify;
                
        current_time = time (NULL);
        tm = localtime (&current_time);
        first_notify = g_strdup_printf ("%02d:%02d",
        tm->tm_hour, tm->tm_min);

        usn_tokens = g_strsplit (usn, "::", -1);
        g_assert (usn_tokens != NULL && usn_tokens[0] != NULL);

        uuid = usn_tokens[0] + 5; /* skip the prefix 'uuid:' */

        if (usn_tokens[1]) {
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
        
        if (device_type)
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
resource_unavailable_cb (GSSDPResourceBrowser *resource_browser,
                         const char           *usn)
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
on_use_filter_radiobutton_toggled (GtkToggleButton *togglebutton,
                gpointer         user_data)
{
        GtkWidget *filter_hbox;
        gboolean use_filter;

        filter_hbox = GTK_WIDGET(gtk_builder_get_object (builder, "address-filter-hbox"));
        g_assert (filter_hbox != NULL);
        
        use_filter = gtk_toggle_button_get_active (togglebutton);
        gtk_widget_set_sensitive (filter_hbox, use_filter);
}

static char *
get_ip_filter ()
{
        int i;
        char *ip;
        guint8 quad[4];

        ip = g_malloc (MAX_IP_LEN);
        for (i=0; i<4; i++) {
                GtkWidget *entry;
                char entry_name[16];
                gint val;

                sprintf (entry_name, "address-entry%d", i);
                entry = GTK_WIDGET(gtk_builder_get_object (builder, entry_name));
                g_assert (entry != NULL);

                val = atoi (gtk_entry_get_text (GTK_ENTRY (entry)));
                quad[i] = CLAMP (val, 0, 255);
        }
        sprintf (ip, "%u.%u.%u.%u", quad[0], quad[1], quad[2], quad[3]);
        
        return ip;
}

G_MODULE_EXPORT
void
on_address_filter_dialog_response (GtkDialog *dialog,
                gint       response,
                gpointer   user_data)
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

static GtkTreeModel *
create_packet_treemodel (void)
{
        GtkListStore *store;

        store = gtk_list_store_new (6,
                        G_TYPE_STRING,
                        G_TYPE_STRING,
                        G_TYPE_STRING,
                        G_TYPE_STRING,
                        SOUP_TYPE_MESSAGE_HEADERS,
                        G_TYPE_POINTER);

        return GTK_TREE_MODEL (store);
}

static GtkTreeModel *
create_device_treemodel (void)
{
        GtkListStore *store;

        store = gtk_list_store_new (4,
                        G_TYPE_STRING,
                        G_TYPE_STRING,
                        G_TYPE_STRING,
                        G_TYPE_STRING);

        return GTK_TREE_MODEL (store);
}

static void
setup_treeview (GtkWidget *treeview,
                GtkTreeModel *model,
                char *headers[])
{
        int i;

        /* Set-up columns */
        for (i=0; headers[i] != NULL; i++) {
                GtkCellRenderer *renderer;
               
                renderer = gtk_cell_renderer_text_new ();
                gtk_tree_view_insert_column_with_attributes (
                                GTK_TREE_VIEW (treeview),
                                -1,
                                headers[i],
                                renderer,
                                "text", i,
                                NULL);
        }

        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
                        model);
}

static void
setup_treeviews ()
{
        GtkWidget *treeviews[2];
        GtkTreeModel *treemodels[2];
        char *headers[2][6] = { {"Time",
                "Source Address",
                "Packet Type",
                "Packet Information",
                NULL }, {"Unique Identifier",
                "First Notify",
                "Device Type",
                "Location",
                NULL } }; 
        GtkTreeSelection *selection;
        int i;

        treeviews[0] = GTK_WIDGET(gtk_builder_get_object (builder,
                        "packet-treeview"));
        g_assert (treeviews[0] != NULL);
        treeviews[1] = GTK_WIDGET(gtk_builder_get_object (builder,
                        "device-details-treeview"));
        g_assert (treeviews[1] != NULL);
        
        treemodels[0] = create_packet_treemodel ();
        g_assert (treemodels[0] != NULL);
        treemodels[1] = create_device_treemodel ();
        g_assert (treemodels[1] != NULL);

        for (i=0; i<2; i++)
                setup_treeview (treeviews[i], treemodels[i], headers[i]);
        
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeviews[0]));
        g_assert (selection != NULL);
        g_signal_connect (selection,
                        "changed",
                        G_CALLBACK (on_packet_selected),
                        (gpointer *) treeviews[0]);
}

G_MODULE_EXPORT
gboolean
on_delete_event (GtkWidget *widget,
                GdkEvent  *event,
                gpointer   user_data)
{
        gtk_main_quit ();
        return TRUE;
}

static gboolean
init_ui (gint *argc, gchar **argv[])
{
        GtkWidget *main_window;
        gint window_width, window_height;
        gchar *ui_path = NULL;
        
        gtk_init (argc, argv);

        /* Try to fetch the ui file from the CWD first */
        ui_path = UI_FILE;
        if (!g_file_test (ui_path, G_FILE_TEST_EXISTS)) {
                /* Then Try to fetch it from the system path */
                ui_path = UI_DIR "/" UI_FILE;

                if (!g_file_test (ui_path, G_FILE_TEST_EXISTS))
                        ui_path = NULL;
        }
        
        if (ui_path == NULL) {
                g_critical ("Unable to load the GUI file %s", UI_FILE);
                return FALSE;
        }

        builder = gtk_builder_new();
        if (gtk_builder_add_from_file(builder, ui_path, NULL) == 0)
                return FALSE;

        main_window = GTK_WIDGET(gtk_builder_get_object (builder, "main-window"));
        g_assert (main_window != NULL);

        /* 80% of the screen but don't get bigger than 1000x800 */
        window_width = CLAMP ((gdk_screen_width () * 80 / 100), 10, 1000);
        window_height = CLAMP ((gdk_screen_height () * 80 / 100), 10, 800);
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
        client = gssdp_client_new (NULL, NULL, &error);
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
