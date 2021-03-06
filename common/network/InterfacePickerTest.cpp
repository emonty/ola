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
 * InterfacePickerTest.cpp
 * Test fixture for the InterfacePicker class
 * Copyright (C) 2005-2009 Simon Newton
 */

#include <cppunit/extensions/HelperMacros.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "ola/network/InterfacePicker.h"
#include "ola/Logging.h"

using ola::network::IPV4Address;
using ola::network::Interface;
using ola::network::InterfacePicker;
using std::cout;
using std::endl;
using std::string;
using std::vector;

class InterfacePickerTest: public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(InterfacePickerTest);
  CPPUNIT_TEST(testGetInterfaces);
  CPPUNIT_TEST(testChooseInterface);
  CPPUNIT_TEST_SUITE_END();

  public:
    void testGetInterfaces();
    void testChooseInterface();
};


class MockPicker: public InterfacePicker {
  public:
    explicit MockPicker(const vector<Interface> &interfaces)
        : InterfacePicker(),
          m_interfaces(interfaces) {}
    std::vector<Interface> GetInterfaces() const { return m_interfaces; }
  private:
    const vector<Interface> &m_interfaces;
};

CPPUNIT_TEST_SUITE_REGISTRATION(InterfacePickerTest);


/*
 * Check that we find at least one candidate interface.
 */
void InterfacePickerTest::testGetInterfaces() {
  InterfacePicker *picker = InterfacePicker::NewPicker();
  vector<Interface> interfaces = picker->GetInterfaces();
  CPPUNIT_ASSERT(interfaces.size() > 0);

  vector<Interface>::iterator iter;
  cout << endl;
  for (iter = interfaces.begin(); iter != interfaces.end(); ++iter) {
    cout << iter->name << endl;
    cout << " ip: " << iter->ip_address << endl;
    cout << " bcast: " << iter->bcast_address << endl;
    cout << " subnet: " << iter->subnet_mask << endl;
    cout << " hw_addr: ";
    for (unsigned int i = 0; i < ola::network::MAC_LENGTH; i++) {
      if (i)
        cout << ':';
      cout << std::setw(2) << std::setfill('0') << std::hex <<
        0 + (uint8_t) iter->hw_address[i];
    }
    cout << endl;
    cout << "---------------" << endl;
  }
  delete picker;
}


void InterfacePickerTest::testChooseInterface() {
  vector<Interface> interfaces;
  MockPicker picker(interfaces);

  // no interfaces
  Interface iface;
  CPPUNIT_ASSERT(!picker.ChooseInterface(&iface, ""));

  // now with one iface that doesn't match
  Interface iface1;
  iface1.name = "eth0";
  CPPUNIT_ASSERT(IPV4Address::FromString("10.0.0.1", &iface1.ip_address));
  interfaces.push_back(iface1);
  CPPUNIT_ASSERT(picker.ChooseInterface(&iface, "192.168.1.1"));
  CPPUNIT_ASSERT(iface1 == iface);

  // check that preferred works
  Interface iface2;
  iface2.name = "eth1";
  CPPUNIT_ASSERT(IPV4Address::FromString("192.168.1.1", &iface2.ip_address));
  interfaces.push_back(iface2);
  CPPUNIT_ASSERT(picker.ChooseInterface(&iface, "192.168.1.1"));
  CPPUNIT_ASSERT(iface2 == iface);

  // now check for iface name
  CPPUNIT_ASSERT(picker.ChooseInterface(&iface, "eth0"));
  CPPUNIT_ASSERT(iface1 == iface);

  CPPUNIT_ASSERT(picker.ChooseInterface(&iface, "eth1"));
  CPPUNIT_ASSERT(iface2 == iface);

  // a invalid address should return the first one
  CPPUNIT_ASSERT(picker.ChooseInterface(&iface, "foo"));
  CPPUNIT_ASSERT(iface1 == iface);
}
