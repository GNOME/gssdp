/*
 * Copyright (C) 2012 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <jensg@openismus.com>
 *
 * This file is part of GSSDP.
 *
 * GSSDP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GSSDP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// Work-around gnome bug 670673; remove once fixed
namespace GSSDP {
[CCode (cheader_filename = "libgssdp/gssdp.h", cprefix = "GSSDP_ERROR_")]
	public errordomain Error {
		NO_IP_ADDRESS,
		FAILED;
		public static GLib.Quark quark ();
	}
}
