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

#include "gssdp-root-device.h"

#ifndef __GSSDP_ROOT_DEVICE_PRIVATE_H__
#define __GSSDP_ROOT_DEVICE_PRIVATE_H__

G_BEGIN_DECLS

void
gssdp_root_device_add_device    (GSSDPRootDevice *root_device,
                                 GSSDPDevice     *device);

void
gssdp_root_device_remove_device (GSSDPRootDevice *root_device,
                                 GSSDPDevice     *device);

G_END_DECLS

#endif /* __GSSDP_ROOT_DEVICE_PRIVATE_H__ */
