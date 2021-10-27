#!/usr/bin/env python3
#
# Copyright (c) 2019, Jens Georg <mail@jensge.org>
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#         SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

import gi
gi.require_version('GSSDP', '1.6')
from gi.repository import GSSDP
from gi.repository import GLib
import time

c = GSSDP.Client (uda_version = GSSDP.UDAVersion.VERSION_1_1, boot_id = time.time(), config_id = 1)
c.init()
g = GSSDP.ResourceGroup(client = c)

def on_update():
    new_boot_id = c.props.boot_id + 1;
    print('Updating boot_id from {} to {}'.format(c.props.boot_id, new_boot_id))
    g.update (c.props.boot_id + 1)
    return True

if __name__ == '__main__':
    g.add_resource_simple ('upnp:rootdevice', 'a66a9a18-3f31-4d41-ad64-98443e4d4399::upnp:rootdevice', 'http://127.0.0.1')
    g.set_available(True)

    l = GLib.MainLoop()
    GLib.timeout_add_seconds(5, on_update)
    l.run()
