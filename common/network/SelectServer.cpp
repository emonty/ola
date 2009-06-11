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
 * SelectServer.cpp
 * Implementation of the SelectServer class
 * Copyright (C) 2005-2008 Simon Newton
 */

#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <algorithm>

#include <lla/Logging.h>
#include <lla/network/SelectServer.h>
#include <lla/network/Socket.h>

using namespace lla::network;

const string SelectServer::K_SOCKET_VAR = "ss-sockets";
const string SelectServer::K_CONNECTED_SOCKET_VAR = "ss-connected-sockets";
const string SelectServer::K_TIMER_VAR = "ss-timer-functions";

using lla::ExportMap;
using lla::Closure;

/*
 * Constructor
 */
SelectServer::SelectServer(ExportMap *export_map):
  m_terminate(false),
  m_export_map(export_map) {

  if (m_export_map) {
    lla::IntegerVariable *var = m_export_map->GetIntegerVar(K_SOCKET_VAR);
    var = m_export_map->GetIntegerVar(K_TIMER_VAR);
  }
}


/*
 * Run the select server until Termiate() is called.
 */
int SelectServer::Run() {
  while (!m_terminate) {
    // false indicates an error in CheckForEvents();
    if (!CheckForEvents())
      break;
  }
}


/*
 * Register a Socket with the select server.
 * @param Socket the socket to register. The OnData closure of this socket will
 *   be called when there is data available for reading.
 * @return true on success, false on failure.
 */
bool SelectServer::AddSocket(Socket *socket) {
  if (socket->ReadDescriptor() == Socket::INVALID_SOCKET) {
    LLA_WARN << "AddSocket failed, fd: " << socket->ReadDescriptor();
    return false;
  }

  vector<Socket*>::const_iterator iter;
  for (iter = m_sockets.begin(); iter != m_sockets.end(); ++iter) {
    if ((*iter)->ReadDescriptor() == socket->ReadDescriptor()) {
      LLA_WARN << "While trying add to add " << socket->ReadDescriptor() <<
        ", fd already exists in the list of read fds";
      return false;
    }
  }

  m_sockets.push_back(socket);
  if (m_export_map)
    m_export_map->GetIntegerVar(K_SOCKET_VAR)->Increment();
  return true;
}


/*
 * Register a ConnectedSocket with the select server.
 * @param socket the socket to register. The OnData method will be called when
 * there is data available for reading. Additionally, OnClose will be called
 *   if the other end closes the connection
 * @param delete_on_close controls whether the select server deletes the socket
 *   once it's closed.
 * @return true on sucess, false on failure.
 */
bool SelectServer::AddSocket(ConnectedSocket *socket,
                             bool delete_on_close) {
  if (socket->ReadDescriptor() == Socket::INVALID_SOCKET) {
    LLA_WARN << "AddSocket failed, fd: " << socket->ReadDescriptor();
    return false;
  }

  connected_socket_t registered_socket;
  registered_socket.socket = socket;
  registered_socket.delete_on_close = delete_on_close;

  vector<connected_socket_t>::const_iterator iter;
  for (iter = m_connected_sockets.begin(); iter != m_connected_sockets.end();
       ++iter) {
    if (iter->socket->ReadDescriptor() == socket->ReadDescriptor()) {
      LLA_WARN << "While trying add to add " << socket->ReadDescriptor() <<
        ", fd already exists in the list of read fds";
      return false;
    }
  }

  m_connected_sockets.push_back(registered_socket);
  if (m_export_map)
    m_export_map->GetIntegerVar(K_CONNECTED_SOCKET_VAR)->Increment();
  return true;
}


/*
 * Unregister a Socket with the select server
 * @param socket the Socket to remove
 * @return true if removed successfully, false otherwise
 */
bool SelectServer::RemoveSocket(Socket *socket) {
  if (socket->ReadDescriptor() == Socket::INVALID_SOCKET)
    LLA_WARN << "Removing a closed socket: " << socket->ReadDescriptor();

  vector<Socket*>::iterator iter;
  for (iter = m_sockets.begin(); iter != m_sockets.end(); ++iter) {
    if ((*iter)->ReadDescriptor() == socket->ReadDescriptor()) {
      m_sockets.erase(iter);
      if (m_export_map)
        m_export_map->GetIntegerVar(K_SOCKET_VAR)->Decrement();
      return true;
    }
  }
  LLA_WARN << "Socket " << socket->ReadDescriptor() << " not found in list";
  return false;
}


/*
 * Unregister a ConnectedSocket with the select server
 * @param socket the Socket to remove
 * @return true if removed successfully, false otherwise
 */
bool SelectServer::RemoveSocket(ConnectedSocket *socket) {
  if (socket->ReadDescriptor() == Socket::INVALID_SOCKET)
    LLA_WARN << "Removing a closed socket: " << socket->ReadDescriptor();

  vector<connected_socket_t>::iterator iter;
  for (iter = m_connected_sockets.begin(); iter != m_connected_sockets.end();
       ++iter) {
    if (iter->socket->ReadDescriptor() == socket->ReadDescriptor()) {
      m_connected_sockets.erase(iter);
      if (m_export_map)
        m_export_map->GetIntegerVar(K_CONNECTED_SOCKET_VAR)->Decrement();
      return true;
    }
  }
  LLA_WARN << "Socket " << socket->ReadDescriptor() << " not found in list";
  return false;
}


/*
 * Register a repeating timeout function. Returning 0 from the closure will
 * cancel this timeout.
 * @param seconds the delay between function calls
 * @param closure the closure to call when the event triggers. Ownership is
 * given up to the select server - make sure nothing else uses this closure.
 */
bool SelectServer::RegisterRepeatingTimeout(int ms, lla::Closure *closure) {
  return RegisterTimeout(ms, closure, true);
}


/*
 * Register a single use timeout function.
 * @param seconds the delay between function calls
 * @param closure the closure to call when the event triggers
 */
bool SelectServer::RegisterSingleTimeout(int ms,
                                         lla::SingleUseClosure *closure) {
  return RegisterTimeout(ms, closure, false);
}


bool SelectServer::RegisterTimeout(int ms,
                                   BaseClosure *closure,
                                   bool repeating) {
  if (!closure)
    return false;

  event_t event;
  event.closure = closure;
  event.interval.tv_sec = ms / K_MS_IN_SECOND;
  event.interval.tv_usec = K_MS_IN_SECOND * (ms % K_MS_IN_SECOND);
  event.repeating = repeating;

  gettimeofday(&event.next, NULL);
  timeradd(&event.next, &event.interval, &event.next);
  m_events.push(event);

  if (m_export_map) {
    lla::IntegerVariable *var = m_export_map->GetIntegerVar(K_TIMER_VAR);
    var->Increment();
  }
  return true;
}


/*
 * One iteration of the select() loop.
 * @return false on error, true on success.
 */
bool SelectServer::CheckForEvents() {
  int maxsd, ret;
  unsigned int i;
  fd_set r_fds, w_fds;
  struct timeval tv;
  struct timeval now;

  maxsd = 0;
  FD_ZERO(&r_fds);
  FD_ZERO(&w_fds);
  AddSocketsToSet(r_fds, maxsd);
  now = CheckTimeouts();

  if (m_events.empty()) {
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  } else {
    struct timeval next = m_events.top().next;
    long long now_l = (long long) now.tv_sec * K_US_IN_SECOND + now.tv_usec;
    long long next_l = (long long) next.tv_sec * K_US_IN_SECOND + next.tv_usec;
    long rem = next_l - now_l;
    tv.tv_sec = rem / K_US_IN_SECOND;
    tv.tv_usec = rem % K_US_IN_SECOND;
  }

  switch (select(maxsd+1, &r_fds, &w_fds, NULL, &tv)) {
    case 0:
      // timeout
      return true;
    case -1:
      if (errno == EINTR)
        return true;
      LLA_WARN << "select() error, " << strerror(errno);
      return false;
    default:
      CheckTimeouts();
      CheckSockets(r_fds);
  }
  return true;
}


/*
 * Add all the read sockets to the FD_SET
 */
void SelectServer::AddSocketsToSet(fd_set &set, int &max_sd) const {
  vector<Socket*>::const_iterator iter;
  for (iter = m_sockets.begin(); iter != m_sockets.end(); ++iter) {
    if ((*iter)->ReadDescriptor() == Socket::INVALID_SOCKET) {
      // The socket was probably closed without removing it from the select
      // server
      LLA_WARN << "Not adding an invalid socket";
      continue;
    }
    max_sd = max(max_sd, (*iter)->ReadDescriptor());
    FD_SET((*iter)->ReadDescriptor(), &set);
  }

  vector<connected_socket_t>::const_iterator con_iter;
  for (con_iter = m_connected_sockets.begin();
       con_iter != m_connected_sockets.end(); ++con_iter) {
    if (con_iter->socket->ReadDescriptor() == Socket::INVALID_SOCKET) {
      // The socket was probably closed without removing it from the select
      // server
      LLA_WARN << "Not adding an invalid socket";
      continue;
    }
    max_sd = max(max_sd, con_iter->socket->ReadDescriptor());
    FD_SET(con_iter->socket->ReadDescriptor(), &set);
  }
}


/*
 * Check all the registered sockets:
 *  - Execute the callback for sockets with data
 *  - Excute OnClose if a remote end closed the connection
 */
void SelectServer::CheckSockets(fd_set &set) {
  // Because the callbacks can add or remove sockets from the select server, we
  // have to call them after we've used the iterators.
  m_ready_queue.clear();

  vector<Socket*>::iterator iter;
  for (iter = m_sockets.begin(); iter != m_sockets.end(); ++iter) {
    if (FD_ISSET((*iter)->ReadDescriptor(), &set)) {
      if ((*iter)->OnData())
        m_ready_queue.push_back((*iter)->OnData());
      else
        LLA_FATAL << "Socket " << (*iter)->ReadDescriptor() <<
          "is ready but no handler attached, this is bad!";
    }
  }

  vector<connected_socket_t>::iterator con_iter;
  for (con_iter = m_connected_sockets.begin();
       con_iter != m_connected_sockets.end(); ++con_iter) {
    if (FD_ISSET(con_iter->socket->ReadDescriptor(), &set)) {
      if (con_iter->socket->CheckIfClosed()) {
        if (con_iter->delete_on_close)
          delete con_iter->socket;
        if (m_export_map)
          m_export_map->GetIntegerVar(K_CONNECTED_SOCKET_VAR)->Decrement();
        con_iter = m_connected_sockets.erase(con_iter);
        con_iter--;
      } else {
        if (con_iter->socket->OnData())
          m_ready_queue.push_back(con_iter->socket->OnData());
        else
          LLA_FATAL << "Socket " << con_iter->socket->ReadDescriptor() <<
            "is ready but no handler attached, this is bad!";
      }
    }
  }

  vector<Closure*>::iterator socket_iter;
  for (socket_iter = m_ready_queue.begin(); socket_iter != m_ready_queue.end();
      ++socket_iter) {
    (*socket_iter)->Run();
  }
}


/*
 * Check for expired timeouts and call them.
 * @returns a struct timeval of the time up to where we checked.
 */
struct timeval SelectServer::CheckTimeouts() {
  struct timeval now;
  gettimeofday(&now, NULL);

  event_t e;
  if (m_events.empty())
    return now;

  for (e = m_events.top(); !m_events.empty() && timercmp(&e.next, &now, <);
       e = m_events.top()) {
    int return_code = 1;
    if (e.closure) {
      return_code = e.closure->Run();
    }
    m_events.pop();

    if (e.repeating && !return_code) {
      e.next = now;
      timeradd(&e.next, &e.interval, &e.next);
      m_events.push(e);
    } else {
      // if we were repeating and we returned an error we need to call delete
      if (e.repeating && !return_code)
        delete e.closure;

      if (m_export_map) {
        lla::IntegerVariable *var = m_export_map->GetIntegerVar(K_TIMER_VAR);
        var->Decrement();
      }
    }
    gettimeofday(&now, NULL);
  }
  return now;
}


/*
 * Remove all registrations.
 */
void SelectServer::UnregisterAll() {
  vector<connected_socket_t>::iterator iter;
  for (iter = m_connected_sockets.begin(); iter != m_connected_sockets.end();
       ++iter) {
    if (iter->delete_on_close) {
      delete iter->socket;
    }
  }
  m_sockets.clear();
  m_connected_sockets.clear();

  while (!m_events.empty()) {
    event_t event = m_events.top();
    if (event.repeating)
      delete event.closure;
    m_events.pop();
  }
}