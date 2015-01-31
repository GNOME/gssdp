/*
 * Copyright (C) 2012 Openismus GmbH
 *
 * Author: Jens Georg <jensg@openismus.com>
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

#ifndef TESTUTIL_H
#define TESTUTIL_H

#include <libgssdp/gssdp-resource-browser.h>

G_BEGIN_DECLS


gboolean
quit_loop (gpointer user_data);

gboolean
unref_object (gpointer object);

void
on_resource_available_assert_not_reached (GSSDPResourceBrowser *src,
                                          const char           *usn,
                                          GList                *locations,
                                          gpointer              user_data);

void
on_resource_unavailable_assert_not_reached (GSSDPResourceBrowser *src,
                                            const char           *usn,
                                            gpointer              user_data);

GSSDPClient *
get_client (GError **error);

G_END_DECLS

#endif // TESTUTIL_H
