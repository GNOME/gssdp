#! /bin/sh
#
#  Copyright (C) 2006 OpenedHand Ltd.
#  Copyright (C) 2010 Intel.
#
#  Author: Jorn Baayen <jorn@openedhand.com>
#          Neil Roberts <neil@linux.intel.com>
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Library General Public
#  License as published by the Free Software Foundation; either
#  version 2 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Library General Public License for more details.
#
#  You should have received a copy of the GNU Library General Public
#  License along with this library; if not, write to the
#  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#  Boston, MA 02110-1301, USA.

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

olddir=`pwd`
cd "$srcdir"

gtkdocize || exit 1
ACLOCAL="${ACLOCAL-aclocal} $ACLOCAL_FLAGS" autoreconf -v --install || exit 1

cd "$olddir"
if test -z "$NOCONFIGURE"; then
    "$srcdir/configure" --enable-maintainer-mode --enable-debug "$@"
fi
