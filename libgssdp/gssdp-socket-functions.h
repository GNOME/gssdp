/*
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
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

#ifndef __GSSDP_SOCKET_FUNCTIONS_H
#define __GSSDP_SOCKET_FUNCTIONS_H

#include <gio/gio.h>

G_GNUC_INTERNAL gboolean
gssdp_socket_enable_loop         (GSocket       *socket,
                                  gboolean       enable,
                                  GError       **error);
G_GNUC_INTERNAL gboolean
gssdp_socket_set_ttl             (GSocket       *socket,
                                  int            ttl,
                                  GError       **error);
G_GNUC_INTERNAL gboolean
gssdp_socket_enable_broadcast    (GSocket       *socket,
                                  gboolean       enable,
                                  GError       **error);
G_GNUC_INTERNAL gboolean
gssdp_socket_mcast_interface_set (GSocket       *socket,
                                  GInetAddress  *iface_address,
                                  GError       **error);
G_GNUC_INTERNAL gboolean
gssdp_socket_mcast_group_join    (GSocket       *socket,
                                  GInetAddress  *group,
                                  GInetAddress  *iface,
                                  GError       **error);
G_GNUC_INTERNAL gboolean
gssdp_socket_reuse_address       (GSocket *socket,
                                  gboolean enable,
                                  GError **error);
#endif
