/*
 * Copyright (C) 2012 Openismus GmbH
 *
 * Author: Jens Georg <jensg@openismus.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
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
