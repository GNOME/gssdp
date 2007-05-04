#include <libgssdp/gssdp.h>
#include <libgssdp/gssdp-client-private.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <string.h>

#define GLADE_FILE "gssdp-device-sniffer.glade"

GladeXML *glade_xml;
GSSDPResourceBrowser *resource_browser;
GSSDPClient *client;

void
on_av_media_servers_1_0_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void
on_av_renderers_1_0_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void
on_enable_packet_capture_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void
on_search_all_devices_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void
on_clear_packet_capture1_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void
on_details_activate (GtkWidget *scrolled_window, GtkCheckMenuItem *menuitem)
{
        gboolean active;

        active = gtk_check_menu_item_get_active (menuitem);
        g_object_set (G_OBJECT (scrolled_window), "visible", active, NULL);
}

void
on_filter_menuitem_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void
on_address_filter_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void
on_search_root_devices_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void
on_show_device_tracking_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void
on_internet_gateways_1_0_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

static void
append_packet (char **packet_data)
{
        GtkWidget *treeview;
        GtkListStore *liststore;
        GtkTreeIter iter;
        
        treeview = glade_xml_get_widget (glade_xml, "packet-treeview");
        g_assert (treeview != NULL);
        liststore = GTK_LIST_STORE (
                        gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));
        g_assert (liststore != NULL);
        
        gtk_list_store_prepend (liststore, &iter);
        gtk_list_store_set (liststore, &iter, 
                        0, packet_data[0],
                        1, packet_data[1],
                        2, packet_data[2],
                        3, packet_data[3],
                        -1); 
}

static char *message_types[] = {"M-SEARCH", "RESPONSE", "NOTIFY"};

static void
on_gssdp_message (GSSDPClient *client,
                const gchar *from_ip,
                _GSSDPMessageType type,
                GHashTable *headers,
                gpointer user_data)
{
        char **packet_data;
        time_t current_time;
        struct tm *tm;
        GSList *node;

        if (type == _GSSDP_DISCOVERY_RESPONSE)
                return;

        packet_data = g_malloc (sizeof (char *) * 5);
        /* Set the Time */
        current_time = time (NULL);
        tm = localtime (&current_time);
        packet_data[0] = g_strdup_printf ("%02d:%02d", tm->tm_hour, tm->tm_min);

        /* Now the Source Address */
        packet_data[1] = g_strdup (from_ip);
        
        /* Now the Packet Type */
        packet_data[2] = g_strdup (message_types[type]);
        
        /* Now the Packet Information */
        if (type == _GSSDP_DISCOVERY_REQUEST)
                node = g_hash_table_lookup (headers, "ST");
        else
                node = g_hash_table_lookup (headers, "NT");
        
        if (node)
                packet_data[3] = g_strdup (node->data);
        packet_data[4] = NULL;

        append_packet (packet_data);
        g_strfreev (packet_data);
}

void
on_custom_search_dialog_response (GtkDialog *dialog,
                gint       response,
                gpointer   user_data)
{
        GtkWidget *entry;

        entry = glade_xml_get_widget (glade_xml, "search-target-entry");
        g_assert (entry != NULL);
        gtk_widget_hide (GTK_WIDGET (dialog));
        if (response == GTK_RESPONSE_OK) {
                g_print ("search target: %s\n",
                                gtk_entry_get_text (GTK_ENTRY (entry)));
        }
        gtk_entry_set_text (GTK_ENTRY (entry), "");
}

static GtkTreeModel *
create_model (void)
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
setup_treeview (GtkWidget *treeview, char *headers[])
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
                        create_model ());
}

static void
setup_treeviews ()
{
        GtkWidget *treeviews[2];
        char *headers[2][6] = { {"Time",
                "Source Address",
                "Packet Type",
                "Packet Information",
                NULL }, {"Address",
                "Notify",
                "Last Notify",
                "Device Type",
                "Unique Identifier",
                NULL } }; 
        int i;

        treeviews[0] = glade_xml_get_widget (glade_xml,
                        "packet-treeview");
        treeviews[1] = glade_xml_get_widget (glade_xml, 
                        "device-details-treeview");

        g_assert (treeviews[0] != NULL && treeviews[1] != NULL);

        for (i=0; i<2; i++)
                setup_treeview (treeviews[i], headers[i]);
}

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
        
        gtk_init (argc, argv);
        glade_init ();

        /* Try to fetch the glade file from the CWD first */
        glade_xml = glade_xml_new (GLADE_FILE, NULL, NULL); 
        if (glade_xml == NULL) {
                /* Then Try to fetch it from the system path */
                glade_xml = glade_xml_new (UI_DIR "/" GLADE_FILE, NULL, NULL);
                if (glade_xml == NULL) {
                        g_error ("Unable to load the GUI file %s", GLADE_FILE);
                        return FALSE;
                }
        }

        main_window = glade_xml_get_widget (glade_xml, "main-window");
        g_assert (main_window != NULL);

        glade_xml_signal_autoconnect (glade_xml);
        setup_treeviews ();
        gtk_widget_show_all (main_window);

        return TRUE;
}

static void
deinit_ui (void)
{
        g_object_unref (glade_xml);
}

static gboolean
init_upnp (void)
{
        GError *error;
        
        g_thread_init (NULL);

        error = NULL;
        client = gssdp_client_new (NULL, &error);
        if (error) {
                g_critical (error->message);
                g_error_free (error);
                return 1;
        }

        resource_browser = gssdp_resource_browser_new (client, "ssdp:all");
        
        g_signal_connect (client,
                          "message-received",
                          G_CALLBACK (on_gssdp_message),
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
        
        deinit_ui ();
        deinit_upnp ();
        
        return 0;
}
