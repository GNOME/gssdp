/* gssdp-device-sniffer-window.c
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

#include "libgssdp/gssdp-client-private.h"
#include "gssdp-device-sniffer-window.h"

#include <libsoup/soup.h>
#include <libgssdp/gssdp.h>


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

static void
on_trigger_rescan (GSimpleAction *action,
                   GVariant *parameter,
                   gpointer user_data);

static void
on_about (GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
on_set_address_filter (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data);

static void
on_show_address_filter (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data);

static void
on_details_activate (GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data);

static void
on_clear_packet_capture (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data);

static void
on_packet_capture_activate (GSimpleAction *action,
                            GVariant *parameter,
                            gpointer user_data);

static void
on_ssdp_message (GssdpDeviceSnifferWindow *self,
                 const gchar              *from_ip,
                 gushort                   from_port,
                 _GSSDPMessageType         type,
                 SoupMessageHeaders       *headers,
                 GSSDPClient              *client);

static void
on_resource_available (GssdpDeviceSnifferWindow *self,
                       const char           *usn,
                       GList                *locations,
                       GSSDPResourceBrowser *ssdp_resource_browser);

static void
on_resource_unavailable (GssdpDeviceSnifferWindow *self,
                         const char           *usn,
                         GSSDPResourceBrowser *ssdp_resource_browser);

static GActionEntry actions[] = {
        { "clear-packet-capture", on_clear_packet_capture },
        { "capture-packets", NULL, NULL, "true", on_packet_capture_activate },
        { "trigger-rescan", on_trigger_rescan },
        { "set-address-filter", on_set_address_filter },
        { "show-packet-details", NULL, NULL, "true", on_details_activate },
        { "show-address-filter", on_show_address_filter },
        { "about", on_about }
};

struct _GssdpDeviceSnifferWindow
{
  GtkApplicationWindow  parent_instance;

  /* Template widgets */
  GtkTreeView          *packet_treeview;
  GtkTreeView          *device_details_treeview;
  GtkMenuButton        *window_menu;

  GtkTextView          *packet_details_textview;

  /* UPnP stuff */
  GSSDPClient          *client;
  GSSDPResourceBrowser *browser;
  char                 *ip_filter;
  gboolean              capture_packets;
};

G_DEFINE_TYPE (GssdpDeviceSnifferWindow, gssdp_device_sniffer_window, GTK_TYPE_APPLICATION_WINDOW)

#define SNIFFER_WINDOW_TEMPLATE "/org/gnome/GssdpDeviceSniffer/gssdp-device-sniffer-window.ui"
#define SNIFFER_WINDOW_MENU_RESOURCE "/org/gnome/GssdpDeviceSniffer/window-menu.ui"

static void
on_trigger_rescan (GSimpleAction *action,
                   GVariant *parameter,
                   gpointer user_data)
{
        GssdpDeviceSnifferWindow *self = GSSDP_DEVICE_SNIFFER_WINDOW (user_data);

        gssdp_resource_browser_rescan (self->browser);
}

static void
on_about (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
#if 0
        GtkWidget *dialog = NULL;

        dialog = GTK_WIDGET (gtk_builder_get_object (builder,
                                                     "about-dialog"));
        gtk_widget_show (dialog);
#endif
}

static void
on_set_address_filter (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
}

static void
on_show_address_filter (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
}

static void
on_details_activate (GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data)
{
}

static void
on_packet_capture_activate (GSimpleAction *action,
                            GVariant *parameter,
                            gpointer user_data)
{
        GssdpDeviceSnifferWindow *self = GSSDP_DEVICE_SNIFFER_WINDOW (user_data);

        self->capture_packets = g_variant_get_boolean (parameter);
        g_simple_action_set_state (action, parameter);
}

static void
on_clear_packet_capture (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
        GssdpDeviceSnifferWindow *self = GSSDP_DEVICE_SNIFFER_WINDOW (user_data);
        GtkTreeModel *model;
        GtkTreeIter iter;
        gboolean more;
        time_t *arrival_time;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->packet_treeview));
        more = gtk_tree_model_get_iter_first (model, &iter);

        while (more) {
                gtk_tree_model_get (model,
                                    &iter,
                                    PACKET_STORE_COLUMN_RAW_ARRIVAL_TIME, &arrival_time, -1);
                g_free (arrival_time);
                more = gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
}

static void
packet_header_to_string (const char *header_name,
                         const char *header_val,
                         GString **text)
{
        g_string_append_printf (*text,
                                "%s: %s\n",
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
update_packet_details (GssdpDeviceSnifferWindow *self,
                       const char *text, unsigned int len)
{
        GtkTextBuffer *textbuffer;

        textbuffer = gtk_text_view_get_buffer (self->packet_details_textview);

        clear_textbuffer (textbuffer);
        gtk_text_buffer_insert_at_cursor (textbuffer, text, len);
}


static void
display_packet (GssdpDeviceSnifferWindow *self,
                time_t arrival_time,
                SoupMessageHeaders *packet_headers)
{
        GString *text;

        text = g_string_new ("");
        g_string_printf (text, "Received on: %s\nHeaders:\n\n",
                        ctime (&arrival_time));

        soup_message_headers_foreach (packet_headers,
                        (SoupMessageHeadersForeachFunc)
                        packet_header_to_string,
                        &text);

        update_packet_details (self, text->str, text->len);
        g_string_free (text, TRUE);
}


static void
on_packet_selected (GssdpDeviceSnifferWindow *self,
                    GtkTreeSelection      *selection)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        time_t *arrival_time;

        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                SoupMessageHeaders *packet_headers;

                gtk_tree_model_get (model,
                                    &iter,
                                    PACKET_STORE_COLUMN_HEADERS,
                                        &packet_headers,
                                    PACKET_STORE_COLUMN_RAW_ARRIVAL_TIME,
                                        &arrival_time,
                                    -1);
                display_packet (self, *arrival_time, packet_headers);
                g_boxed_free (SOUP_TYPE_MESSAGE_HEADERS, packet_headers);
        }

        else
                update_packet_details (self, "", 0);
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
gssdp_device_sniffer_window_class_init (GssdpDeviceSnifferWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               SNIFFER_WINDOW_TEMPLATE);
  gtk_widget_class_bind_template_child (widget_class,
                                        GssdpDeviceSnifferWindow,
                                        packet_treeview);
  gtk_widget_class_bind_template_child (widget_class,
                                        GssdpDeviceSnifferWindow,
                                        device_details_treeview);
  gtk_widget_class_bind_template_child (widget_class,
                                        GssdpDeviceSnifferWindow,
                                        window_menu);
  gtk_widget_class_bind_template_child (widget_class,
                                        GssdpDeviceSnifferWindow,
                                        packet_details_textview);
}

static void
gssdp_device_sniffer_window_init (GssdpDeviceSnifferWindow *self)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GtkTreeSelection *selection;
  GtkWidget *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, SNIFFER_WINDOW_MENU_RESOURCE, &error);

  gtk_menu_button_set_menu_model (self->window_menu,
                                 G_MENU_MODEL (gtk_builder_get_object (builder,
                                                                       "sniffer-window-menu")));
  g_action_map_add_action_entries (G_ACTION_MAP (self), actions, G_N_ELEMENTS (actions), self);


  selection = gtk_tree_view_get_selection (self->packet_treeview);
  g_signal_connect_swapped (selection,
                            "changed",
                            G_CALLBACK (on_packet_selected),
                            self);

  menu = gtk_menu_new_from_model (G_MENU_MODEL (gtk_builder_get_object (builder,
                                                                        "sniffer-context-menu")));
  g_object_unref (builder);

  gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (self->packet_treeview), NULL);
  g_signal_connect (G_OBJECT (self->packet_treeview),
                    "button-press-event",
                    G_CALLBACK (on_treeview_popup_menu),
                    menu);

  /* SSDP initialization */
  self->client = g_initable_new (GSSDP_TYPE_CLIENT,
                                 NULL,
                                 &error,
                                 //"address-family", G_SOCKET_FAMILY_IPV4,
                                 //"interface", NULL,
                                 NULL);
  self->browser = gssdp_resource_browser_new (self->client,
                                              GSSDP_ALL_RESOURCES);

  g_signal_connect_swapped (self->client,
                            "message-received",
                            G_CALLBACK (on_ssdp_message),
                            self);
  g_signal_connect_swapped (self->browser,
                            "resource-available",
                            G_CALLBACK (on_resource_available),
                            self);

  g_signal_connect_swapped (self->browser,
                            "resource-unavailable",
                            G_CALLBACK (on_resource_unavailable),
                            self);

  self->capture_packets = TRUE;
  gssdp_resource_browser_set_active (self->browser, TRUE);
}

static const char *message_types[] = {"M-SEARCH", "RESPONSE", "NOTIFY"};

static char **
packet_to_treeview_data (GssdpDeviceSnifferWindow *self,
                         const gchar        *from_ip,
                         time_t              arrival_time,
                         _GSSDPMessageType   type,
                         SoupMessageHeaders *headers)
{
        char **packet_data;
        const char *target;
        struct tm *tm;

        packet_data = g_new0 (char *, 6);

        /* Set the Time */
        tm = localtime (&arrival_time);
        packet_data[0] = g_strdup_printf ("%02d:%02d", tm->tm_hour, tm->tm_min);

        /* Now the Source Address */
        packet_data[1] = g_strdup (from_ip);

        packet_data[2] = g_strdup (gssdp_client_get_interface (self->client));

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
append_packet (GssdpDeviceSnifferWindow *self,
               const gchar *from_ip,
               time_t arrival_time,
               _GSSDPMessageType type,
               SoupMessageHeaders *headers)
{
        GtkListStore *liststore;
        GtkTreeIter iter;
        char **packet_data;

        liststore = GTK_LIST_STORE (
                        gtk_tree_view_get_model (self->packet_treeview));

        packet_data = packet_to_treeview_data (self,
                                               from_ip,
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
                        PACKET_STORE_COLUMN_RAW_ARRIVAL_TIME,
                                g_memdup (&arrival_time, sizeof (time_t)),
                        -1);
        g_strfreev (packet_data);
}


static void
on_ssdp_message (GssdpDeviceSnifferWindow *self,
                 const gchar              *from_ip,
                 gushort                   from_port,
                 _GSSDPMessageType         type,
                 SoupMessageHeaders       *headers,
                 GSSDPClient              *client)
{
        time_t arrival_time;

        arrival_time = time (NULL);

        if (type == _GSSDP_DISCOVERY_REQUEST)
                return;

        if (self->ip_filter != NULL && strcmp (self->ip_filter, from_ip) != 0)
                return;

        if (!self->capture_packets)
                return;

        append_packet (self, from_ip, arrival_time, type, headers);
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
append_device (GssdpDeviceSnifferWindow *self,
               const char *uuid,
               const char *first_notify,
               const char *device_type,
               const char *location)
{
        GtkTreeModel *model;
        GtkTreeIter iter;

        model = gtk_tree_view_get_model (self->device_details_treeview);

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
on_resource_available (GssdpDeviceSnifferWindow *self,
                       const char           *usn,
                       GList                *locations,
                       GSSDPResourceBrowser *ssdp_resource_browser)
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

        if (usn_tokens[1] && strlen(usn_tokens[1]) != 0) {
                char **urn_tokens;

                urn_tokens = g_strsplit (usn_tokens[1], ":device:", -1);

                if (urn_tokens[1])
                        device_type = g_strdup (urn_tokens[1]);
                g_strfreev (urn_tokens);
        }

        /* Device Announcement */
        append_device (self,
                       uuid,
                       first_notify,
                       device_type,
                       (char *) locations->data);

        g_free (device_type);
        g_free (first_notify);
        g_strfreev (usn_tokens);
}

static void
on_resource_unavailable (GssdpDeviceSnifferWindow *self,
                         const char           *usn,
                         GSSDPResourceBrowser *ssdp_resource_browser)
{
}
