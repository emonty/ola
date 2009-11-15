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
 * E131PDU.cpp
 * The E131PDU
 * Copyright (C) 2007-2009 Simon Newton
 */


#include <string.h>
#include <ola/Logging.h>
#include <ola/network/NetworkUtils.h>
#include "E131PDU.h"
#include "DMPPDU.h"

namespace ola {
namespace e131 {

using ola::network::HostToNetwork;

/*
 * Size of the header portion.
 */
unsigned int E131PDU::HeaderSize() const {
  return sizeof(E131Header::e131_pdu_header);
}


/*
 * Size of the data portion
 */
unsigned int E131PDU::DataSize() const {
  if (m_dmp_pdu)
    return m_dmp_pdu->Size();
  return 0;
}


/*
 * Pack the header portion.
 */
bool E131PDU::PackHeader(uint8_t *data, unsigned int &length) const {
  if (length < sizeof(E131Header::e131_pdu_header)) {
    OLA_WARN << "E131PDU::PackHeader: buffer too small, got " << length <<
      " required " << sizeof(E131Header::e131_pdu_header);
    length = 0;
    return false;
  }

  E131Header::e131_pdu_header *header = (E131Header::e131_pdu_header*) data;
  strncpy(header->source, m_header.Source().data(),
          E131Header::SOURCE_NAME_LEN);
  header->sequence = m_header.Sequence();
  header->priority = m_header.Priority();
  header->universe = HostToNetwork(m_header.Universe());
  length = sizeof(E131Header::e131_pdu_header);
  return true;
}


/*
 * Pack the data portion.
 */
bool E131PDU::PackData(uint8_t *data, unsigned int &length) const {
  if (m_dmp_pdu)
    return m_dmp_pdu->Pack(data, length);
  length = 0;
  return true;
}


} // e131
} // ola