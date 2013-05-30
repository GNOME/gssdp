/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GSSDP_ERROR_H__
#define __GSSDP_ERROR_H__

#include <glib.h>

G_BEGIN_DECLS

GQuark
gssdp_error_quark (void) G_GNUC_CONST;

#define GSSDP_ERROR (gssdp_error_quark ())

/**
 * GSSDPError:
 * @GSSDP_ERROR_NO_IP_ADDRESS: GSSDP could not find a valid IP address of a
 * #GSSDPClient.
 * @GSSDP_ERROR_FAILED: Unknown error.
 */
typedef enum {
        GSSDP_ERROR_NO_IP_ADDRESS,
        GSSDP_ERROR_FAILED
} GSSDPError;

G_END_DECLS

#endif /* __GSSDP_ERROR_H__ */
