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
 * RobeWidgetTest.cpp
 * Test fixture for the DmxterWidget class
 * Copyright (C) 2010 Simon Newton
 */

#include <string.h>
#include <cppunit/extensions/HelperMacros.h>
#include <queue>
#include <string>
#include <vector>

#include "ola/BaseTypes.h"
#include "ola/Callback.h"
#include "ola/DmxBuffer.h"
#include "ola/Logging.h"
#include "ola/rdm/UID.h"
#include "plugins/usbpro/BaseRobeWidget.h"
#include "plugins/usbpro/RobeWidget.h"
#include "plugins/usbpro/CommonWidgetTest.h"


using ola::DmxBuffer;
using ola::plugin::usbpro::BaseRobeWidget;
using ola::plugin::usbpro::RobeWidget;
using ola::rdm::GetResponseFromData;
using ola::rdm::DiscoveryUniqueBranchRequest;
using ola::rdm::MuteRequest;
using ola::rdm::RDMDiscoveryCommand;
using ola::rdm::RDMRequest;
using ola::rdm::RDMResponse;
using ola::rdm::UID;
using ola::rdm::UnMuteRequest;
using std::auto_ptr;
using std::string;
using std::vector;


class RobeWidgetTest: public CommonWidgetTest {
  CPPUNIT_TEST_SUITE(RobeWidgetTest);
  CPPUNIT_TEST(testSendDMX);
  CPPUNIT_TEST(testSendRDMRequest);
  CPPUNIT_TEST(testMuteDevice);
  CPPUNIT_TEST(testUnMuteAll);
  CPPUNIT_TEST(testReceive);
  CPPUNIT_TEST(testBranch);
  CPPUNIT_TEST_SUITE_END();

  public:
    void setUp();

    void testSendDMX();
    void testSendRDMRequest();
    void testMuteDevice();
    void testUnMuteAll();
    void testBranch();
    void testReceive();

  private:
    auto_ptr<ola::plugin::usbpro::RobeWidget> m_widget;
    uint8_t m_transaction_number;
    ola::rdm::rdm_response_code m_received_code;
    bool m_new_dmx_data;

    void Terminate() {
      m_ss.Terminate();
    }

    void NewDMXData() {
      m_new_dmx_data = true;
      m_ss.Terminate();
    }

    const RDMRequest *NewRequest(const UID &destination,
                                 const uint8_t *data = NULL,
                                 unsigned int length = 0);
    uint8_t *PackRDMRequest(const RDMRequest *request, unsigned int *size);
    uint8_t *PackDiscoveryReqest(const RDMDiscoveryCommand *request,
                                 unsigned int *size);
    uint8_t *PackRDMResponse(const RDMResponse *response, unsigned int *size);
    void ValidateResponse(ola::rdm::rdm_response_code code,
                          const ola::rdm::RDMResponse *response,
                          const vector<string> &packets);
    void ValidateStatus(ola::rdm::rdm_response_code expected_code,
                        vector<string> expected_packets,
                        ola::rdm::rdm_response_code code,
                        const ola::rdm::RDMResponse *response,
                        const vector<string> &packets);
    void ValidateMuteStatus(bool expected,
                            bool actual);
    void ValidateBranchStatus(const uint8_t *expected_data,
                              unsigned int length,
                              const uint8_t *actual_data,
                              unsigned int actual_length);
    void CallbackComplete() { m_ss.Terminate(); }

    static const UID BCAST_DESTINATION;
    static const UID DESTINATION;
    static const UID SOURCE;
    static const uint16_t ESTA_ID = 0x7890;
    static const uint32_t SERIAL_NUMBER = 0x01020304;
    static const uint8_t DMX_IN_REQUEST_LABEL = 0x04;
    static const uint8_t DMX_IN_RESPONSE_LABEL = 0x05;
    static const uint8_t DMX_FRAME_LABEL = 0x06;
    static const uint8_t TEST_RDM_DATA[];
    static const uint8_t TEST_MUTE_RESPONSE_DATA[];
    static const unsigned int FOOTER_SIZE = 1;
    static const unsigned int HEADER_SIZE = 5;
    static const unsigned int PADDING_SIZE = 4;
};


const UID RobeWidgetTest::SOURCE(1, 2);
const UID RobeWidgetTest::DESTINATION(ESTA_ID, SERIAL_NUMBER);
const UID RobeWidgetTest::BCAST_DESTINATION(ESTA_ID, 0xffffffff);
const uint8_t RobeWidgetTest::TEST_RDM_DATA[] = {0x5a, 0x5a, 0x5a, 0x5a};
const uint8_t RobeWidgetTest::TEST_MUTE_RESPONSE_DATA[] = {0, 0};


CPPUNIT_TEST_SUITE_REGISTRATION(RobeWidgetTest);


void RobeWidgetTest::setUp() {
  CommonWidgetTest::setUp();
  m_widget.reset(
      new ola::plugin::usbpro::RobeWidget(&m_descriptor, &m_ss, SOURCE));
  m_transaction_number = 0;
  m_new_dmx_data = false;
}


/*
 * Helper method to create new GetRDMRequest objects.
 * @param destination the destination UID
 * @param data the RDM Request data
 * @param length the size of the RDM data.
 */
const RDMRequest *RobeWidgetTest::NewRequest(const UID &destination,
                                             const uint8_t *data,
                                             unsigned int length) {
  return new ola::rdm::RDMGetRequest(
      SOURCE,
      destination,
      m_transaction_number++,  // transaction #
      1,  // port id
      0,  // message count
      10,  // sub device
      296,  // param id
      data,
      length);
}


/**
 * Pack a RDM request into a buffer
 */
uint8_t *RobeWidgetTest::PackRDMRequest(const RDMRequest *request,
                                        unsigned int *size) {
  unsigned int request_size = request->Size() + PADDING_SIZE;
  uint8_t *rdm_data = new uint8_t[request_size];
  memset(rdm_data, 0, request_size);
  CPPUNIT_ASSERT(request->Pack(
        rdm_data,
        &request_size));
  *size = request_size + PADDING_SIZE;
  return rdm_data;
}


/**
 * Pack a RDM Discovery Command
 */
uint8_t *RobeWidgetTest::PackDiscoveryReqest(
    const RDMDiscoveryCommand *request,
    unsigned int *size) {
  unsigned int request_size = request->Size() + PADDING_SIZE;
  uint8_t *rdm_data = new uint8_t[request_size];
  memset(rdm_data, 0, request_size);
  CPPUNIT_ASSERT(request->Pack(
        rdm_data,
        &request_size));
  *size = request_size + PADDING_SIZE;
  return rdm_data;
}


/**
 * Pack a RDM Response into a buffer
 */
uint8_t *RobeWidgetTest::PackRDMResponse(const RDMResponse *response,
                                         unsigned int *size) {
  unsigned int response_size = response->Size() + PADDING_SIZE;
  uint8_t *rdm_data = new uint8_t[response_size];
  memset(rdm_data, 0, response_size);
  CPPUNIT_ASSERT(response->Pack(
        rdm_data,
        &response_size));
  *size = response_size + PADDING_SIZE;
  return rdm_data;
}


/*
 * Check the response matches what we expected.
 */
void RobeWidgetTest::ValidateResponse(
    ola::rdm::rdm_response_code code,
    const ola::rdm::RDMResponse *response,
    const vector<string> &packets) {
  CPPUNIT_ASSERT_EQUAL(ola::rdm::RDM_COMPLETED_OK, code);
  CPPUNIT_ASSERT(response);
  CPPUNIT_ASSERT_EQUAL(
      static_cast<unsigned int>(sizeof(TEST_RDM_DATA)),
      response->ParamDataSize());
  CPPUNIT_ASSERT(0 == memcmp(TEST_RDM_DATA, response->ParamData(),
                             response->ParamDataSize()));

  CPPUNIT_ASSERT_EQUAL((size_t) 1, packets.size());
  ola::rdm::rdm_response_code raw_code;
  auto_ptr<ola::rdm::RDMResponse> raw_response(
    ola::rdm::RDMResponse::InflateFromData(packets[0], &raw_code));
  CPPUNIT_ASSERT(*(raw_response.get()) == *response);
  delete response;
  m_ss.Terminate();
}


/*
 * Check that this request returned the expected status code
 * @param expected_code the expected widget status code
 * @param expected_code a list of expected packets
 * @param code the actual status code returns
 * @param response the RDMResponse object, or NULL
 * @param packets the actual packets involved
 */
void RobeWidgetTest::ValidateStatus(
    ola::rdm::rdm_response_code expected_code,
    vector<string> expected_packets,
    ola::rdm::rdm_response_code code,
    const ola::rdm::RDMResponse *response,
    const vector<string> &packets) {
  CPPUNIT_ASSERT_EQUAL(expected_code, code);
  CPPUNIT_ASSERT(!response);

  CPPUNIT_ASSERT_EQUAL(expected_packets.size(), packets.size());
  for (unsigned int i = 0; i < packets.size(); i++) {
    if (expected_packets[i].size() != packets[i].size())
      OLA_INFO << expected_packets[i].size() << " != " << packets[i].size();
    CPPUNIT_ASSERT_EQUAL(expected_packets[i].size(), packets[i].size());

    if (expected_packets[i] != packets[i]) {
      for (unsigned int j = 0; j < packets[i].size(); j++) {
        OLA_INFO << std::hex << static_cast<int>(packets[i][j]) << " - " <<
          static_cast<int>(expected_packets[i][j]);
      }
    }
    CPPUNIT_ASSERT(expected_packets[i] == packets[i]);
  }
  m_received_code = expected_code;
  m_ss.Terminate();
}


/**
 * Validate that a mute response matches what we expect
 */
void RobeWidgetTest::ValidateMuteStatus(bool expected,
                                        bool actual) {
  CPPUNIT_ASSERT_EQUAL(expected, actual);
  m_ss.Terminate();
}


void RobeWidgetTest::ValidateBranchStatus(const uint8_t *expected_data,
                                          unsigned int length,
                                          const uint8_t *actual_data,
                                          unsigned int actual_length) {
  CPPUNIT_ASSERT_EQUAL(length, actual_length);
  CPPUNIT_ASSERT(!memcmp(expected_data, actual_data, length));
  m_ss.Terminate();
}


/*
 * Check that we can send DMX
 */
void RobeWidgetTest::testSendDMX() {
  // dmx data
  DmxBuffer buffer;
  buffer.SetFromString("0,1,2,3,4");

  // expected message
  uint8_t dmx_frame_data[] = {0, 1, 2, 3, 4, 0, 0, 0, 0};
  // add the expected data, run and verify.
  m_endpoint->AddExpectedRobeMessage(
      DMX_FRAME_LABEL,
      dmx_frame_data,
      sizeof(dmx_frame_data),
      ola::NewSingleCallback(this, &RobeWidgetTest::Terminate));
  m_widget->SendDMX(buffer);
  m_ss.Run();
  m_endpoint->Verify();

  // now test an empty frame
  DmxBuffer buffer2;
  uint8_t empty_frame_data[] = {0, 0, 0, 0};  // null frames still have 4 bytes

  // add the expected data, run and verify.
  m_endpoint->AddExpectedRobeMessage(
      DMX_FRAME_LABEL,
      empty_frame_data,
      sizeof(empty_frame_data),
      ola::NewSingleCallback(this, &RobeWidgetTest::Terminate));
  m_widget->SendDMX(buffer2);
  m_ss.Run();
  m_endpoint->Verify();
}


/**
 * Check that we send RDM messages correctly.
 */
void RobeWidgetTest::testSendRDMRequest() {
  // request
  const RDMRequest *rdm_request = NewRequest(DESTINATION);
  unsigned int expected_request_frame_size;
  uint8_t *expected_request_frame = PackRDMRequest(
      rdm_request,
      &expected_request_frame_size);

  // response
  auto_ptr<const RDMResponse> response(
    GetResponseFromData(rdm_request, TEST_RDM_DATA, sizeof(TEST_RDM_DATA)));
  unsigned int response_size;
  uint8_t *response_frame = PackRDMResponse(response.get(), &response_size);

  // add the expected response, send and verify
  m_endpoint->AddExpectedRobeDataAndReturn(
      BaseRobeWidget::RDM_REQUEST,
      expected_request_frame,
      expected_request_frame_size,
      BaseRobeWidget::RDM_RESPONSE,
      response_frame,
      response_size);

  m_widget->SendRDMRequest(
      rdm_request,
      ola::NewSingleCallback(this, &RobeWidgetTest::ValidateResponse));
  m_ss.Run();
  m_endpoint->Verify();

  delete[] expected_request_frame;
  delete[] response_frame;

  // now check broadcast messages
  // request
  rdm_request = NewRequest(BCAST_DESTINATION);
  uint8_t *expected_bcast_request_frame = PackRDMRequest(
      rdm_request,
      &expected_request_frame_size);

  // add the expected response, send and verify
  m_endpoint->AddExpectedRobeMessage(
      BaseRobeWidget::RDM_REQUEST,
      expected_bcast_request_frame,
      expected_request_frame_size,
      ola::NewSingleCallback(this, &RobeWidgetTest::Terminate));

  vector<string> packets;
  // This is a bit confusing, the ValidateStatus is invoked immediately, but
  // we still need to call m_ss.Run() to ensure the correct packet was sent.
  m_widget->SendRDMRequest(
      rdm_request,
      ola::NewSingleCallback(this,
                             &RobeWidgetTest::ValidateStatus,
                             ola::rdm::RDM_WAS_BROADCAST,
                             packets));
  CPPUNIT_ASSERT_EQUAL(ola::rdm::RDM_WAS_BROADCAST, m_received_code);
  m_ss.Run();
  m_endpoint->Verify();

  // cleanup time
  delete[] expected_bcast_request_frame;
}


/**
 * Test mute device
 */
void RobeWidgetTest::testMuteDevice() {
  // first test when a device doesn't respond
  const MuteRequest mute_request(SOURCE,
                                 DESTINATION,
                                 m_transaction_number++);
  unsigned int expected_request_frame_size;
  uint8_t *expected_request_frame = PackDiscoveryReqest(
      &mute_request,
      &expected_request_frame_size);

  // response, we get PADDING_SIZE bytes when nothing else is returned
  uint8_t response_frame[PADDING_SIZE];
  memset(response_frame, 0, PADDING_SIZE);

  // add the expected response, send and verify
  m_endpoint->AddExpectedRobeDataAndReturn(
      BaseRobeWidget::RDM_REQUEST,
      expected_request_frame,
      expected_request_frame_size,
      BaseRobeWidget::RDM_RESPONSE,
      response_frame,
      PADDING_SIZE);

  m_widget.get()->m_impl->MuteDevice(
      DESTINATION,
      ola::NewSingleCallback(this,
                             &RobeWidgetTest::ValidateMuteStatus,
                             false));
  m_ss.Run();
  m_endpoint->Verify();
  delete[] expected_request_frame;

  // now try an actual mute response
  const MuteRequest mute_request2(SOURCE,
                                  DESTINATION,
                                  m_transaction_number++);
  expected_request_frame = PackDiscoveryReqest(
      &mute_request2,
      &expected_request_frame_size);

  // We can really return anything as long as it's > 4 bytes
  // TODO(simon): make this better
  uint8_t mute_response_frame[] = {0, 0, 0, 0, 0, 0};

  // add the expected response, send and verify
  m_endpoint->AddExpectedRobeDataAndReturn(
      BaseRobeWidget::RDM_REQUEST,
      expected_request_frame,
      expected_request_frame_size,
      BaseRobeWidget::RDM_RESPONSE,
      mute_response_frame,
      sizeof(mute_response_frame));

  m_widget.get()->m_impl->MuteDevice(
      DESTINATION,
      ola::NewSingleCallback(this,
                             &RobeWidgetTest::ValidateMuteStatus,
                             true));
  m_ss.Run();
  m_endpoint->Verify();
  delete[] expected_request_frame;
}


/**
 * Test the unmute all request works
 */
void RobeWidgetTest::testUnMuteAll() {
  const UnMuteRequest unmute_request(SOURCE,
                                     UID::AllDevices(),
                                     m_transaction_number++);
  unsigned int expected_request_frame_size;
  uint8_t *expected_request_frame = PackDiscoveryReqest(
      &unmute_request,
      &expected_request_frame_size);

  // response, we get PADDING_SIZE bytes when nothing else is returned
  uint8_t response_frame[PADDING_SIZE];
  memset(response_frame, 0, PADDING_SIZE);

  // add the expected response, send and verify
  m_endpoint->AddExpectedRobeDataAndReturn(
      BaseRobeWidget::RDM_REQUEST,
      expected_request_frame,
      expected_request_frame_size,
      BaseRobeWidget::RDM_RESPONSE,
      response_frame,
      PADDING_SIZE);

  m_widget.get()->m_impl->UnMuteAll(
      ola::NewSingleCallback(this,
                             &RobeWidgetTest::CallbackComplete));
  m_ss.Run();
  m_endpoint->Verify();
  delete[] expected_request_frame;
}


/**
 * Test the DUB request works
 */
void RobeWidgetTest::testBranch() {
  // first test when no devices respond
  const DiscoveryUniqueBranchRequest discovery_request(
      SOURCE,
      UID(0, 0),
      UID::AllDevices(),
      m_transaction_number++);
  unsigned int expected_request_frame_size;
  uint8_t *expected_request_frame = PackDiscoveryReqest(
      &discovery_request,
      &expected_request_frame_size);

  // response, we get PADDING_SIZE bytes when nothing else is returned
  uint8_t response_frame[PADDING_SIZE];
  memset(response_frame, 0, PADDING_SIZE);

  // add the expected response, send and verify
  m_endpoint->AddExpectedRobeDataAndReturn(
      BaseRobeWidget::RDM_DISCOVERY,
      expected_request_frame,
      expected_request_frame_size,
      BaseRobeWidget::RDM_DISCOVERY_RESPONSE,
      NULL,
      0);

  m_widget.get()->m_impl->Branch(
      UID(0, 0),
      UID::AllDevices(),
      ola::NewSingleCallback(this,
                             &RobeWidgetTest::ValidateBranchStatus,
                             static_cast<const uint8_t*>(NULL),
                             static_cast<unsigned int>(0)));
  m_ss.Run();
  m_endpoint->Verify();
  delete[] expected_request_frame;

  // now try an actual response, the data doesn't actually have to be valid
  // because it's just passed straight to the callback.
  const DiscoveryUniqueBranchRequest discovery_request2(
      SOURCE,
      UID(0, 0),
      UID::AllDevices(),
      m_transaction_number++);
  expected_request_frame = PackDiscoveryReqest(
      &discovery_request2,
      &expected_request_frame_size);

  // the response, can be anything really, last 4 bytes is trimmed
  uint8_t response_frame2[] = {1, 2, 3, 4, 0, 0, 0, 0};

  // add the expected response, send and verify
  m_endpoint->AddExpectedRobeDataAndReturn(
      BaseRobeWidget::RDM_DISCOVERY,
      expected_request_frame,
      expected_request_frame_size,
      BaseRobeWidget::RDM_DISCOVERY_RESPONSE,
      response_frame2,
      sizeof(response_frame2));

  m_widget.get()->m_impl->Branch(
      UID(0, 0),
      UID::AllDevices(),
      ola::NewSingleCallback(
        this,
        &RobeWidgetTest::ValidateBranchStatus,
        static_cast<const uint8_t*>(response_frame2),
        // minus the 4 padding bytes
        static_cast<unsigned int>(sizeof(response_frame2) - 4)));
  m_ss.Run();
  m_endpoint->Verify();
  delete[] expected_request_frame;
}


/*
 * Test receiving works.
 */
void RobeWidgetTest::testReceive() {
  DmxBuffer buffer;
  buffer.SetFromString("0,1,2,3,4");

  // change to recv mode & setup the callback
  m_endpoint->AddExpectedRobeMessage(
      DMX_IN_REQUEST_LABEL,
      NULL,
      0,
      ola::NewSingleCallback(this, &RobeWidgetTest::Terminate));
  m_widget->ChangeToReceiveMode();
  m_ss.Run();
  m_endpoint->Verify();
  m_widget->SetDmxCallback(
      ola::NewCallback(this, &RobeWidgetTest::NewDMXData));
  CPPUNIT_ASSERT(!m_new_dmx_data);

  // now send some data
  m_endpoint->SendUnsolicitedRobeData(DMX_IN_RESPONSE_LABEL,
                                      buffer.GetRaw(),
                                      buffer.Size());
  m_ss.Run();
  CPPUNIT_ASSERT(m_new_dmx_data);
  const DmxBuffer &new_data = m_widget->FetchDMX();
  CPPUNIT_ASSERT(buffer == new_data);
}

