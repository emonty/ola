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
 * UsbProDevice.cpp
 * UsbPro device
 * Copyright (C) 2006-2007 Simon Newton
 *
 * The device creates two ports, one in and one out, but you can only use one at a time.
 */

#include <string.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/service.h>

#include <llad/logger.h>
#include <llad/Preferences.h>

#include "UsbProDevice.h"
#include "UsbProPort.h"

namespace lla {
namespace plugin {

using google::protobuf::RpcController;
using google::protobuf::Closure;
using lla::plugin::usbpro::Request;
using lla::plugin::usbpro::Reply;

/*
 * Create a new device
 *
 * @param owner  the plugin that owns this device
 * @param name  the device name
 * @param dev_path  path to the pro widget
 */
UsbProDevice::UsbProDevice(lla::AbstractPlugin *owner,
                           const string &name,
                           const string &dev_path):
  Device(owner, name),
  m_path(dev_path),
  m_enabled(false),
  m_widget(NULL) {
    m_widget = new UsbProWidget();
}


/*
 * Destroy this device
 */
UsbProDevice::~UsbProDevice() {
  if (m_enabled)
    Stop();

  if (m_widget)
    delete m_widget;
}


/*
 * Start this device
 */
bool UsbProDevice::Start() {
  UsbProPort *port = NULL;
  int ret;

  // connect to the widget
  ret = m_widget->Connect(m_path);

  if (ret) {
    Logger::instance()->log(Logger::WARN, "UsbProPlugin: failed to connect to %s", m_path.c_str());
    return -1;
  }

  m_widget->SetListener(this);

  /* set up ports */
  for (int i=0; i < 2; i++) {
    port = new UsbProPort(this, i);

    if (port)
      AddPort(port);
  }

  m_enabled = true;
  return 0;
}


/*
 * Stop this device
 */
bool UsbProDevice::Stop() {
  if (!m_enabled)
    return false;

  m_widget->Disconnect();
  DeleteAllPorts();
  m_enabled = false;
  return true;
}


/*
 * return the sd for this device
 */
lla::select_server::ConnectedSocket *UsbProDevice::GetSocket() const {
  return m_widget->GetSocket();
}


/*
 * Send the dmx out the widget
 * called from the UsbProPort
 *
 * @return   0 on success, non 0 on failure
 */
int UsbProDevice::SendDmx(uint8_t *data, int len) {
  return m_widget->SendDmx(data,len);
}


/*
 * Copy the dmx buffer into the arguments
 * Called from the UsbProPort
 *
 * @return   the length of the dmx data copied
 */
int UsbProDevice::FetchDmx(uint8_t *data, int len) const {
  return m_widget->FetchDmx(data, len);
}


/*
 * Handle device config messages
 *
 * @param controller An RpcController
 * @param request the request data
 * @param response the response to return
 * @param done the closure to call once the request is complete
 */
void UsbProDevice::Configure(RpcController *controller,
                             const string &request,
                             string *response,
                             Closure *done) {
    Request request_pb;
    if (!request_pb.ParseFromString(request)) {
      controller->SetFailed("Invalid Request");
      done->Run();
      return;
    }

    switch (request_pb.type()) {
      case lla::plugin::usbpro::Request::USBPRO_PARAMETER_REQUEST:
        HandleGetParameters(controller, &request_pb, response, done);
        break;
      case lla::plugin::usbpro::Request::USBPRO_SET_PARAMETER_REQUEST:
        HandleSetParameters(controller, &request_pb, response, done);
        break;
      case lla::plugin::usbpro::Request::USBPRO_SERIAL_REQUEST:
        HandleGetSerial(controller, &request_pb, response, done);
        break;
      default:
        controller->SetFailed("Invalid Request");
        done->Run();
    }
}



/*
 * put the device back into recv mode
 */
int UsbProDevice::ChangeToReceiveMode() {
  m_widget->ChangeToReceiveMode();
  return 0;
}

/*
 * Handle a get params request
 */
void UsbProDevice::HandleGetParameters(RpcController *controller,
                                       const Request *request,
                                       string *response,
                                       Closure *done) {

  if (!m_widget->GetParameters()) {
    controller->SetFailed("GetParameters failed");
    done->Run();
  } else {
    // TODO: we should time these out if we don't get a response
    outstanding_request parameters_request;
    parameters_request.controller = controller;
    parameters_request.response = response;
    parameters_request.done = done;
    m_outstanding_param_requests.push_back(parameters_request);
  }
}


/*
 * Handle a set params request
 */
void UsbProDevice::HandleSetParameters(RpcController *controller,
                                       const Request *request,
                                       string *response,
                                       Closure *done) {

  if (!request->has_set_parameters()) {
    controller->SetFailed("Missing set parameters message");
    done->Run();
    return;
  }

  bool ret = m_widget->SetParameters(NULL,
                                     0,
                                     request->set_parameters().break_time(),
                                     request->set_parameters().mab_time(),
                                     request->set_parameters().rate());

  if (!ret) {
    controller->SetFailed("GetParameters failed");
    done->Run();
    return;
  }

  Reply reply;
  reply.set_type(lla::plugin::usbpro::Reply::USBPRO_SET_PARAMETER_REPLY);
  reply.SerializeToString(response);
  done->Run();

}


void UsbProDevice::HandleGetSerial(RpcController *controller,
                                   const Request *request,
                                   string *response,
                                   Closure *done) {

  if (!m_widget->GetSerial()) {
    controller->SetFailed("GetSerial failed");
    done->Run();
  } else {
    // TODO: we should time these out if we don't get a response
    outstanding_request serial_request;
    serial_request.controller = controller;
    serial_request.response = response;
    serial_request.done = done;
    m_outstanding_serial_requests.push_back(serial_request);
  }
}


/*
 * Called when the dmx changes
 */
void UsbProDevice::NewDmx() {
  AbstractPort *port = GetPort(0);
  port->DmxChanged();
}


/*
 * Called when the GetParameters request returns
 */
void UsbProDevice::Parameters(uint8_t firmware,
                              uint8_t firmware_high,
                              uint8_t break_time,
                              uint8_t mab_time,
                              uint8_t rate) {

  if (!m_outstanding_param_requests.empty()) {
    outstanding_request parameter_request = m_outstanding_param_requests.front();
    m_outstanding_param_requests.pop_front();

    Reply reply;
    reply.set_type(lla::plugin::usbpro::Reply::USBPRO_PARAMETER_REPLY);
    lla::plugin::usbpro::ParameterReply *parameters_reply = reply.mutable_parameters();

    parameters_reply->set_firmware_high(firmware_high);
    parameters_reply->set_firmware(firmware);
    parameters_reply->set_break_time(break_time);
    parameters_reply->set_mab_time(mab_time);
    parameters_reply->set_rate(rate);

    reply.SerializeToString(parameter_request.response);
    parameter_request.done->Run();

  }
}



/*
 * Called when the GetSerial request returns
 */
void UsbProDevice::SerialNumber(const uint8_t serial_number[4]) {
  if (!m_outstanding_serial_requests.empty()) {
    outstanding_request serial_request = m_outstanding_serial_requests.front();
    m_outstanding_serial_requests.pop_front();

    Reply reply;
    reply.set_type(lla::plugin::usbpro::Reply::USBPRO_SERIAL_REPLY);
    lla::plugin::usbpro::SerialNumberReply *serial_reply =
      reply.mutable_serial_number();

    serial_reply->set_serial((char*) serial_number);

    reply.SerializeToString(serial_request.response);
    serial_request.done->Run();
  }
}

} // plugin
} // lla