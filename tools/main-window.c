// SPDX-License-Identifier: LGPL-2.1-or-later

#include "main-window.h"

#include <gio/gio.h>
#include <glib.h>

#include <glib/gi18n.h>

#include <libgssdp/gssdp-client-private.h>
#include <libgssdp/gssdp.h>

#include <libsoup/soup.h>

#define LOGO_RESOURCE "/org/gupnp/Logo.svg"

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

typedef enum {
        DEVICE_STORE_COLUMN_UUID,
        DEVICE_STORE_COLUMN_FIRST_SEEN,
        DEVICE_STORE_COLUMN_TYPE,
        DEVICE_STORE_COLUMN_LOCATION
} DEVICE_STORE_COLUMNS;

enum
{
        PROP_0,
        PROP_INTERFACE,
        PROP_ADDRESS_FAMILY,
        PROP_CAPTURE,
};

struct _GSSDPDeviceSnifferMainWindow {
        GtkApplicationWindow parent_instance;

        // SSDP related parameters
        char *interface;
        GSocketFamily family;
        GSSDPClient *client;
        GSSDPResourceBrowser *browser;
        int packets;
        int devices;

        // Bound child widgets
        GtkWidget *packet_treeview;
        GtkWidget *packet_textview;
        GtkWidget *capture_button;
        GtkWidget *device_treeview;
        GtkWidget *details_scrolled;
        GMenuModel *sniffer_context_menu;
        GtkWidget *searchbar;
        GtkWidget *address_filter;
        GtkWidget *searchbutton;
        GtkWidget *info_label;
        GtkWidget *counter_label;

        // Other widgets
        GtkWidget *context_menu;

        // Options
        gboolean capture;
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
        GSSDPDeviceSnifferMainWindow *self =
                GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (object);

        switch (property_id) {
        case PROP_CAPTURE:
                g_value_set_boolean (value, self->capture);
                break;
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
        case PROP_CAPTURE:
                self->capture = g_value_get_boolean (value);
                const char *icon_name = self->capture
                                    ? "media-playback-stop-symbolic"
                                    : "media-playback-start-symbolic";

                gtk_button_set_icon_name(GTK_BUTTON (self->capture_button), icon_name);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static const char *message_types[] = { "M-SEARCH", "RESPONSE", "NOTIFY" };

static char **
packet_to_treeview_data (const gchar *from_ip,
                         char *client_interface,
                         GDateTime *arrival_time,
                         _GSSDPMessageType type,
                         SoupMessageHeaders *headers)
{
        char **packet_data;
        const char *target;

        packet_data = g_new0 (char *, 6);

        /* Set the Time */
        packet_data[0] = g_date_time_format (arrival_time, "%R");

        /* Now the Source Address */
        packet_data[1] = g_strdup (from_ip);

        packet_data[2] = client_interface;

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
append_packet (GSSDPDeviceSnifferMainWindow *self,
               const gchar *from_ip,
               GDateTime *arrival_time,
               _GSSDPMessageType type,
               SoupMessageHeaders *headers)
{
        GtkListStore *liststore;
        GtkTreeIter iter;
        char **packet_data;

        liststore = GTK_LIST_STORE (gtk_tree_view_get_model (
                GTK_TREE_VIEW (self->packet_treeview)));
        g_assert (liststore != NULL);

        char *client_interface =
                g_strdup (gssdp_client_get_interface (self->client));

        packet_data = packet_to_treeview_data (from_ip,
                                               client_interface,
                                               arrival_time,
                                               type,
                                               headers);

        gtk_list_store_insert_with_values (liststore,
                                           &iter,
                                           0,
                                           PACKET_STORE_COLUMN_TIME,
                                           packet_data[0],
                                           PACKET_STORE_COLUMN_IP,
                                           packet_data[1],
                                           PACKET_STORE_COLUMN_INTERFACE,
                                           packet_data[2],
                                           PACKET_STORE_COLUMN_PACKET_TYPE,
                                           packet_data[3],
                                           PACKET_STORE_COLUMN_TARGET,
                                           packet_data[4],
                                           PACKET_STORE_COLUMN_HEADERS,
                                           headers,
                                           PACKET_STORE_COLUMN_RAW_ARRIVAL_TIME,
                                           arrival_time,
                                           -1);
        g_strfreev (packet_data);
}

static void
update_counter_label (GSSDPDeviceSnifferMainWindow *self)
{
        char *text = g_strdup_printf (_ ("%d packets, %d devices"), self->packets, self->devices);
        gtk_label_set_text (GTK_LABEL (self->counter_label), text);
        g_free (text);
}

static void
on_ssdp_message (GSSDPClient *ssdp_client,
                 const gchar *from_ip,
                 gushort from_port,
                 _GSSDPMessageType type,
                 SoupMessageHeaders *headers,
                 gpointer user_data)
{
        GDateTime *arrival_time;
        GSSDPDeviceSnifferMainWindow *self =
                GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (user_data);

        if (type == _GSSDP_DISCOVERY_REQUEST)
                return;

        if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (self->searchbar))) {
                const char *filter = gtk_editable_get_text (
                        GTK_EDITABLE (self->address_filter));
                if (filter != NULL && strcmp (filter, from_ip) != 0)
                        return;
        }

        if (!self->capture)
                return;

        self->packets++;
        arrival_time = g_date_time_new_now_local ();
        append_packet (self, from_ip, arrival_time, type, headers);
        g_date_time_unref (arrival_time);
        update_counter_label (self);
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

static gboolean
append_device (GtkWidget *treeview,
               const char *uuid,
               const char *first_notify,
               const char *device_type,
               const char *location)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        gboolean result = FALSE;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

        if (!find_device (model, uuid, &iter)) {
                gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                                   &iter, 0,
                                                   DEVICE_STORE_COLUMN_UUID, uuid,
                                                   DEVICE_STORE_COLUMN_FIRST_SEEN, first_notify,
                                                   DEVICE_STORE_COLUMN_LOCATION, location, -1);
                result = TRUE;
        }

        if (device_type) {
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                    DEVICE_STORE_COLUMN_TYPE, device_type, -1);
        }

        return result;
}

static void
resource_available_cb (GSSDPDeviceSnifferMainWindow *self,
                       const char *usn,
                       GList *locations,
                       gpointer user_data)
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
        if (append_device (self->device_treeview,
                           uuid,
                           first_notify,
                           device_type,
                           (char *) locations->data)) {
                self->devices++;
                update_counter_label (self);
        }

        g_free (device_type);
        g_free (first_notify);
        g_strfreev (usn_tokens);
}

static gboolean
remove_device (GtkWidget *treeview, const char *uuid)
{
        GtkTreeModel *model;
        GtkTreeIter iter;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

        if (find_device (model, uuid, &iter)) {
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

                return TRUE;
        }

        return FALSE;
}

static void
resource_unavailable_cb (GSSDPDeviceSnifferMainWindow *self,
                         const char *usn,
                         gpointer user_data)
{
        char **usn_tokens;
        char *uuid;

        usn_tokens = g_strsplit (usn, "::", -1);
        g_assert (usn_tokens != NULL && usn_tokens[0] != NULL);
        uuid = usn_tokens[0] + 5; /* skip the prefix 'uuid:' */

        if (remove_device (self->device_treeview, uuid)) {
                self->devices--;
                update_counter_label (self);
        }

        g_strfreev (usn_tokens);
}

static void
main_window_constructed (GObject *object)
{
        if (G_OBJECT_CLASS (gssdp_device_sniffer_main_window_parent_class) != NULL)
                G_OBJECT_CLASS (gssdp_device_sniffer_main_window_parent_class)
                        ->constructed (object);

        GSSDPDeviceSnifferMainWindow *self =
                GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (object);
        GError *error = NULL;

        self->client = GSSDP_CLIENT (g_initable_new (GSSDP_TYPE_CLIENT,
                                                     NULL,
                                                     &error,
                                                     "address-family",
                                                     self->family,
                                                     "interface",
                                                     self->interface,
                                                     NULL));

        if (error != NULL) {
                g_critical ("Failed to create client: %s", error->message);
                g_clear_error (&error);
                // TODO: Show error dialog
                return;
        }

        self->browser =
                gssdp_resource_browser_new (self->client, GSSDP_ALL_RESOURCES);

        g_signal_connect (self->client,
                          "message-received",
                          G_CALLBACK (on_ssdp_message),
                          self);
        g_signal_connect_swapped (self->browser,
                                  "resource-available",
                                  G_CALLBACK (resource_available_cb),
                                  self);
        g_signal_connect_swapped (self->browser,
                                  "resource-unavailable",
                                  G_CALLBACK (resource_unavailable_cb),
                                  self);

        gssdp_resource_browser_set_active (self->browser, TRUE);

        char *status =
                g_strdup_printf (_ ("Capturing on %s (%s)…"),
                                 gssdp_client_get_interface (self->client),
                                 gssdp_client_get_host_ip (self->client));
        gtk_label_set_text (GTK_LABEL (self->info_label), status);
        g_free (status);
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

        gtk_widget_class_bind_template_child (widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              capture_button);

        gtk_widget_class_bind_template_child (widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              device_treeview);

        gtk_widget_class_bind_template_child (widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              details_scrolled);

        gtk_widget_class_bind_template_child(widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              sniffer_context_menu);

        gtk_widget_class_bind_template_child(widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              searchbar);

        gtk_widget_class_bind_template_child(widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              address_filter);

        gtk_widget_class_bind_template_child(widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              searchbutton);

        gtk_widget_class_bind_template_child (widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              info_label);

        gtk_widget_class_bind_template_child (widget_class,
                                              GSSDPDeviceSnifferMainWindow,
                                              counter_label);

        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->set_property = main_window_set_property;
        object_class->get_property = main_window_get_property;
        object_class->constructed = main_window_constructed;

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

        g_object_class_install_property (
                object_class,
                PROP_CAPTURE,
                g_param_spec_boolean (
                        "capture",
                        "Packet capture state",
                        "Whether or not packet capturing is enabled",
                        TRUE,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                G_PARAM_STATIC_STRINGS));
}

static void
on_realize (GtkWidget *self, gpointer user_data)
{
        double w;
        double h;

        GtkNative *native = gtk_widget_get_native (self);
        GdkSurface *surface = gtk_native_get_surface (native);
        GdkDisplayManager *mgr = gdk_display_manager_get ();
        GdkDisplay *display = gdk_display_manager_get_default_display (mgr);
        GdkMonitor *monitor =
                gdk_display_get_monitor_at_surface (display, surface);
        GdkRectangle rectangle;

        gdk_monitor_get_geometry (monitor, &rectangle);
        w = rectangle.width * 0.75;
        h = rectangle.height * 0.75;

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
on_clear_capture (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
        GSSDPDeviceSnifferMainWindow *self =
                GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (user_data);

        GtkTreeModel *model;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->packet_treeview));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        self->packets = 0;
        update_counter_label (self);
}

static void
on_trigger_rescan (GSimpleAction *action,
                   GVariant *parameter,
                   gpointer user_data)
{
        GSSDPDeviceSnifferMainWindow *self =
                GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (user_data);

        gssdp_resource_browser_rescan (self->browser);
}

static void
on_about (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
        const char *AUTHORS[] = { "Zeeshan Ali (Khattak) <zeeshanak@gnome.org>",
                                  "Jens Georg <mail@jensge.org>",
                                  NULL };

        GdkTexture *logo = gdk_texture_new_from_resource (LOGO_RESOURCE);

        gtk_show_about_dialog (
                GTK_WINDOW (user_data),
                "copyright",
                _ ("Copyright © 2007 Zeeshan Ali (Khattak)"),
                "comments",
                _ ("A Device Sniffer tool based on GSSDP framework. Inspired "
                   "by Intel Tools for UPnP."),
                "authors",
                AUTHORS,
                // TRANSLATORS: Replace this string with your names, one per
                // line
                "translator_credits",
                "translator-credits",
                "license-type",
                GTK_LICENSE_LGPL_2_1,
                "logo", logo,
                NULL);

        g_object_unref (logo);
}

static void
on_set_address_filter (GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data)
{
        GSSDPDeviceSnifferMainWindow *self =
                GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (user_data);

        GtkTreeSelection *selection = NULL;
        GtkTreeModel *model = NULL;
        GtkTreeIter iter;
        char *ip_filter;

        selection = gtk_tree_view_get_selection (
                GTK_TREE_VIEW (self->packet_treeview));
        gtk_tree_selection_get_selected (selection, &model, &iter);
        gtk_tree_model_get (model,
                            &iter,
                            PACKET_STORE_COLUMN_IP,
                            &ip_filter,
                            -1);
        gtk_editable_set_text (GTK_EDITABLE (self->address_filter), ip_filter);
        g_free (ip_filter);

        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self->searchbar), TRUE);
}


static GActionEntry actions[] = { { "clear-capture", on_clear_capture },
                                  { "trigger-rescan", on_trigger_rescan },
                                  { "about", on_about },
                                  { "set-address-filter",
                                    on_set_address_filter } };

static void
on_button_release (GtkGesture *click,
                   int n_press,
                   gdouble x,
                   gdouble y,
                   gpointer user_data)
{
        GSSDPDeviceSnifferMainWindow *self =
                GSSDP_DEVICE_SNIFFER_MAIN_WINDOW (user_data);

        GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence (
                GTK_GESTURE_SINGLE (click));

        GdkEvent *event = gtk_gesture_get_last_event (click, sequence);

        if (n_press != 1) {
                return;
        }

        if (!gdk_event_triggers_context_menu (event)) {
                return;
        }

        gtk_gesture_set_sequence_state (GTK_GESTURE (click), sequence,
                                        GTK_EVENT_SEQUENCE_CLAIMED);
        GtkTreeModel *model;
        GtkTreeIter iter;

        GtkTreeSelection *selection = gtk_tree_view_get_selection (
                GTK_TREE_VIEW (self->packet_treeview));

        if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
                return;
        }

        GdkRectangle rect = { x, y, 1, 1 };
        gtk_popover_set_pointing_to (GTK_POPOVER (self->context_menu), &rect);
        gtk_popover_popup (GTK_POPOVER (self->context_menu));
}

static void
gssdp_device_sniffer_main_window_init (GSSDPDeviceSnifferMainWindow *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->context_menu =
                gtk_popover_menu_new_from_model (self->sniffer_context_menu);
        gtk_widget_set_parent (self->context_menu, self->packet_treeview);
        gtk_popover_set_position (GTK_POPOVER (self->context_menu),
                                  GTK_POS_BOTTOM);
        gtk_popover_set_has_arrow (GTK_POPOVER (self->context_menu), FALSE);
        gtk_widget_set_halign (self->context_menu, GTK_ALIGN_START);

        GtkGesture *click = gtk_gesture_click_new ();
        gtk_event_controller_set_name (GTK_EVENT_CONTROLLER (click),
                                       "packet-treeview-click");
        gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 3);
        gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (click), TRUE);
        gtk_widget_add_controller (self->packet_treeview,
                                   GTK_EVENT_CONTROLLER (click));

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

        g_signal_connect (G_OBJECT (click),
                          "pressed",
                          G_CALLBACK (on_button_release),
                          self);


        GPropertyAction *action =
                g_property_action_new ("toggle-capture", self, "capture");        
        g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
        action = g_property_action_new ("show-packet-details",
                                        self->details_scrolled,
                                        "visible");
        g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));

        g_action_map_add_action_entries (G_ACTION_MAP (self),
                                         actions,
                                         G_N_ELEMENTS (actions),
                                         self);

        g_object_bind_property (self->searchbutton,
                                "active",
                                self->searchbar,
                                "search-mode-enabled",
                                G_BINDING_BIDIRECTIONAL |
                                G_BINDING_SYNC_CREATE);
}
