/* 
 * Copyright (C) 2006 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <glib.h>

#ifndef GSSDP_MACROS_H
#define GSSDP_MACROS_H

#ifndef _GSSDP_API
# define _GSSDP_API
#endif

#define GSSDP_DEPRECATED_FOR(func) _GSSDP_API G_DEPRECATED_FOR(func)
#define GSSDP_DEPRECATED _GSSDP_API G_DEPRECATED

#endif
