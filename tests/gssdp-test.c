/* 
 * (C) 2006 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
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

static void
discoverable_available_cb (GSSDPRootDevice *root_device,
                           const char      *target,
                           const char      *usn,
                           const char      *location)
{
        g_print ("Discoverable available:\n"
                 "\tTarget:\t%s\n"
                 "\tUSN:\t%s\n"
                 "\tLocation:\t%s\n",
                 target,
                 usn,
                 location);
}

static void
discoverable_unavailable_cb (GSSDPRootDevice *root_device,
                             const char      *target,
                             const char      *usn,
                             const char      *location)
{
        g_print ("Discoverable unavailable:\n"
                 "\tTarget:\t%s\n"
                 "\tUSN:\t%s\n",
                 target,
                 usn);
}

int
main (int    argc,
      char **argv)
{
        GSSDPRootDevice *root_device;
        GMainLoop *main_loop;

        g_type_init ();

        root_device = gssdp_root_device_new
                        ("schemas-upnp-org:device:InternetGatewayDevice",
                         1,
                         "http://localhost/");

        gssdp_discoverable_set_available (GSSDP_DISCOVERABLE (root_device),
                                          TRUE);

        g_signal_connect (root_device,
                          "discoverable-available",
                          G_CALLBACK (discoverable_available_cb),
                          NULL);
        g_signal_connect (root_device,
                          "discoverable-unavailable",
                          G_CALLBACK (discoverable_unavailable_cb),
                          NULL);

        gssdp_root_device_discover (root_device,
                                    "upnp:rootdevice");

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (root_device);

        return 0;
}
