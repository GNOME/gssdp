/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
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

#ifndef __GSSDP_SOCKET_SOURCE_H__
#define __GSSDP_SOCKET_SOURCE_H__

G_BEGIN_DECLS

typedef struct _GSSDPSocketSource GSSDPSocketSource;

typedef enum {
        GSSDP_SOCKET_SOURCE_TYPE_REQUEST,
        GSSDP_SOCKET_SOURCE_TYPE_MULTICAST
} GSSDPSocketSourceType;

G_GNUC_INTERNAL GSSDPSocketSource *
gssdp_socket_source_new    (GSSDPSocketSourceType type,
                            const char           *host_ip);

G_GNUC_INTERNAL int
gssdp_socket_source_get_fd (GSSDPSocketSource    *socket_source);

G_END_DECLS

#endif /* __GSSDP_SOCKET_SOURCE_H__ */
