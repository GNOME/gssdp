/*
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#ifndef GSSDP_SOCKET_FUNCTIONS_H
#define GSSDP_SOCKET_FUNCTIONS_H

#include <gio/gio.h>

G_GNUC_INTERNAL gboolean
gssdp_socket_mcast_interface_set (GSocket       *socket,
                                  GInetAddress  *iface_address,
                                  gint           index,
                                  GError       **error);
G_GNUC_INTERNAL gboolean
gssdp_socket_reuse_address       (GSocket *socket,
                                  gboolean enable,
                                  GError **error);

G_GNUC_INTERNAL gboolean
gssdp_socket_enable_info         (GSocket *socket,
                                  GSocketFamily family,
                                  gboolean enable,
                                  GError **error);

#endif
