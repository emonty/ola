#!/usr/bin/python
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Library General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# old_universe_info.py
# Copyright (C) 2005-2009 Simon Newton

"""Lists the active universes."""

__author__ = 'nomis52@gmail.com (Simon Newton)'

import client_wrapper
from ola.OlaClient import Plugin

IN_OR_OUT = {
  True: 'OUT',
  False: 'IN'
}

def Devices(state, devices):
  for device in sorted(devices):
    print 'Device %d: %s' % (device.alias, device.name)
    for port in device.ports:
      print '  port %d, %s %s' % (port.id, IN_OR_OUT[port.is_output], port.description)
  wrapper.Stop()


wrapper = client_wrapper.ClientWrapper()
client = wrapper.Client()
#client.FetchDevices(Devices, Plugin.OLA_PLUGIN_DUMMY)
client.FetchDevices(Devices)
wrapper.Run()