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
 * RobeUsbWidgetTest.cpp
 * Test fixture for the RobeUsbWidget class
 * Copyright (C) 2011 Simon Newton
 */

#include <string.h>
#include <cppunit/extensions/HelperMacros.h>
#include <string>
#include <queue>

#include "ola/Callback.h"
#include "ola/network/NetworkUtils.h"
#include "ola/network/SelectServer.h"
#include "ola/network/Socket.h"
#include "plugins/usbpro/RobeUsbWidget.h"


using ola::plugin::usbpro::RobeUsbWidget;
using ola::network::ConnectedDescriptor;
using ola::network::PipeDescriptor;
using std::string;
using std::queue;


class RobeUsbWidgetTest: public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(RobeUsbWidgetTest);
  CPPUNIT_TEST(testSend);
  CPPUNIT_TEST(testReceive);
  CPPUNIT_TEST(testRemove);
  CPPUNIT_TEST_SUITE_END();

  public:
    void setUp();
    void tearDown();

    void testSend();
    void testReceive();
    void testRemove();

  private:
    ola::network::SelectServer m_ss;
    PipeDescriptor m_descriptor;
    PipeDescriptor *m_other_end;
    RobeUsbWidget *m_widget;
    string m_expected;
    bool m_removed;

    typedef struct {
      uint8_t label;
      unsigned int size;
      const uint8_t *data;
    } expected_message;

    queue<expected_message> m_messages;

    void AddExpected(const string &data);
    void AddExpectedMessage(uint8_t label,
                            unsigned int size,
                            const uint8_t *data);
    void Receive();
    void ReceiveMessage(uint8_t label,
                        const uint8_t *data,
                        unsigned int size);
    void Timeout() { m_ss.Terminate(); }
    void DeviceRemoved() {
      m_removed = true;
      m_ss.Terminate();
    }
};


CPPUNIT_TEST_SUITE_REGISTRATION(RobeUsbWidgetTest);


void RobeUsbWidgetTest::setUp() {
  m_descriptor.Init();
  m_other_end = m_descriptor.OppositeEnd();

  m_ss.AddReadDescriptor(&m_descriptor);
  m_ss.AddReadDescriptor(m_other_end, true);
  m_widget = new RobeUsbWidget(&m_descriptor);
  m_removed = false;

  m_ss.RegisterSingleTimeout(
      30,  // 30ms should be enough
      ola::NewSingleCallback(this, &RobeUsbWidgetTest::Timeout));
}


void RobeUsbWidgetTest::tearDown() {
  delete m_widget;
}


void RobeUsbWidgetTest::AddExpected(const string &data) {
  m_expected.append(data.data(), data.size());
}


void RobeUsbWidgetTest::AddExpectedMessage(uint8_t label,
                                           unsigned int size,
                                           const uint8_t *data) {
  expected_message message = {
    label,
    size,
    data};
  m_messages.push(message);
}


/*
 * Ceceive some data and check it's what we expected.
 */
void RobeUsbWidgetTest::Receive() {
  uint8_t buffer[100];
  unsigned int data_read;

  CPPUNIT_ASSERT(!m_other_end->Receive(buffer, sizeof(buffer), data_read));

  string recieved(reinterpret_cast<char*>(buffer), data_read);
  CPPUNIT_ASSERT(0 == m_expected.compare(0, data_read, recieved));
  m_expected.erase(0, data_read);

  if (m_expected.empty())
    m_ss.Terminate();
}


/**
 * Called when a new message arrives
 */
void RobeUsbWidgetTest::ReceiveMessage(uint8_t label,
                                   const uint8_t *data,
                                   unsigned int size) {
  CPPUNIT_ASSERT(m_messages.size());
  expected_message message = m_messages.front();
  m_messages.pop();

  CPPUNIT_ASSERT_EQUAL(message.label, label);
  CPPUNIT_ASSERT_EQUAL(message.size, size);
  CPPUNIT_ASSERT(!memcmp(message.data, data, size));

  if (m_messages.empty())
    m_ss.Terminate();
}


/**
 * Test sending works
 */
void RobeUsbWidgetTest::testSend() {
  m_other_end->SetOnData(ola::NewCallback(this, &RobeUsbWidgetTest::Receive));

  uint8_t expected[] = {
    0xa5, 0x14, 0, 0, 0xb9, 0x72,
    0xa5, 0x15, 0, 0, 0xba, 0x74,
    0xa5, 0x14, 4, 0, 0xbd, 0xde, 0xad, 0xbe, 0xef, 0xb2,
  };
  AddExpected(string(reinterpret_cast<char*>(expected), sizeof(expected)));
  CPPUNIT_ASSERT(m_widget->SendMessage(0x14, NULL, 0));
  CPPUNIT_ASSERT(m_widget->SendMessage(0x15, NULL, 0));

  uint32_t data = ola::network::HostToNetwork(0xdeadbeef);
  CPPUNIT_ASSERT(m_widget->SendMessage(0x14,
                                       reinterpret_cast<uint8_t*>(&data),
                                       sizeof(data)));

  CPPUNIT_ASSERT(!m_widget->SendMessage(10, NULL, 4));
  m_ss.Run();

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), m_expected.size());
}


/**
 * Test receiving works.
 */
void RobeUsbWidgetTest::testReceive() {
  m_widget->SetMessageHandler(
      ola::NewCallback(this, &RobeUsbWidgetTest::ReceiveMessage));

  uint8_t data[] = {
    0xa5, 1, 0, 0, 0xa6, 0x4c,
    0xa5, 0x0b, 4, 0, 0xb4, 0xde, 0xad, 0xbe, 0xef, 0xa0,
    0xaa, 0xbb,  // some random bytes
    0xa5, 0xff, 0xff, 0xff, 0xe7,  // msg is too long
    0xa5, 0xa, 4, 0, 0xb3, 0xa5, 0xa5, 0xa5, 0xa5, 0xfa,  // data contains 0xa5
    0xa5, 2, 4, 0, 0x00,  // bad checksum
    0xa5, 2, 4, 0, 0xab, 0xde, 0xad, 0xbe, 0xef, 0xaa,  // bad checksum
  };

  uint32_t data_chunk = ola::network::HostToNetwork(0xdeadbeef);
  AddExpectedMessage(1, 0, NULL);
  AddExpectedMessage(0x0b,
                     sizeof(data_chunk),
                     reinterpret_cast<uint8_t*>(&data_chunk));
  uint32_t data_chunk2 = ola::network::HostToNetwork(0xa5a5a5a5);
  AddExpectedMessage(0x0a,
                     sizeof(data_chunk2),
                     reinterpret_cast<uint8_t*>(&data_chunk2));

  ssize_t bytes_sent = m_other_end->Send(data, sizeof(data));
  CPPUNIT_ASSERT_EQUAL(static_cast<ssize_t>(sizeof(data)), bytes_sent);
  m_ss.Run();

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), m_messages.size());
}


/**
 * Test on remove works.
 */
void RobeUsbWidgetTest::testRemove() {
  m_widget->GetDescriptor()->SetOnClose(
      ola::NewSingleCallback(this, &RobeUsbWidgetTest::DeviceRemoved));
  m_other_end->Close();
  m_ss.Run();

  CPPUNIT_ASSERT(m_removed);
}
