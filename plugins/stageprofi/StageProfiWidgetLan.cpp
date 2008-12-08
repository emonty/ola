/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * stageprofiwidget.cpp
 * StageProfi Widget
 * Copyright (C) 2006-2007 Simon Newton
 *
 * The StageProfi LAN Widget.
 */

#include <sys/types.h>

#include <llad/logger.h>
#include <lla/select_server/Socket.h>

#include "StageProfiWidgetLan.h"

#if HAVE_CONFIG_H
#  include <config.h>
#endif

namespace lla {
namespace plugin {

using std::string;
using lla::select_server::TcpSocket;

static const unsigned int STAGEPROFI_PORT = 10001;

/*
 * Connect to the widget
 */
int StageProfiWidgetLan::Connect(const string &ip) {
  TcpSocket *socket = new TcpSocket();
  int ret = socket->Connect(ip, STAGEPROFI_PORT);
  m_socket = socket;
  return ret;
}

} // plugin
} // lla