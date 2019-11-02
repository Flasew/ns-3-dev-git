/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 University of California, San Diego
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:  Weiyang Wang <wew168@ucsd.edu>
 */

#include "tcp-option-tdtcp.h"
#include "ns3/log.h"


namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TcpOptionTdTcpCapable);
NS_OBJECT_ENSURE_REGISTERED (TcpOptionTdTcpDSS);
NS_OBJECT_ENSURE_REGISTERED (TcpOptionTdTcpClose);

NS_LOG_COMPONENT_DEFINE ("TcpOptionTdTcp");


/////////////////////////////////////////////////////////
////////  Base for TDTCP options
//////////////////////////////a///////////////////////////
TcpOptionTdTcpMain::TcpOptionTdTcpMain ()
  : TcpOption ()
{
  NS_LOG_FUNCTION (this);
}

TcpOptionTdTcpMain::~TcpOptionTdTcpMain ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
TcpOptionTdTcpMain::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint8_t
TcpOptionTdTcpMain::GetKind (void) const
{
  return TcpOption::TDTCP;
}

TypeId
TcpOptionTdTcpMain::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpOptionTdTcpMain")
    .SetParent<TcpOption> ()
  ;
  return tid;
}

void
TcpOptionTdTcpMain::Print (std::ostream &os) const
{
  NS_ASSERT_MSG (false, " You should override TcpOptionTdTcp::Print function");
}

Ptr<TcpOption>
TcpOptionTdTcpMain::CreateTdTcpOption (const uint8_t& subtype)
{
  NS_LOG_FUNCTION_NOARGS();
  switch (subtype)
    {
    case TD_CAPABLE:
      return CreateObject<TcpOptionTdTcpCapable>();
    case TD_DSS:
      return CreateObject<TcpOptionTdTcpDSS>();
    case TD_CLOSE:
      return CreateObject<TcpOptionTdTcpClose>();
    default:
      break;
    }

  NS_FATAL_ERROR ("Unsupported TDTCP suboption" << subtype);
  return 0;
}

void
TcpOptionTdTcpMain::SerializeRef (Buffer::Iterator& i) const
{
  i.WriteU8 (GetKind ());
  i.WriteU8 (GetSerializedSize ());
}

uint32_t
TcpOptionTdTcpMain::DeserializeRef (Buffer::Iterator& i) const
{
  uint8_t kind = i.ReadU8 ();
  uint32_t length = 0;

  NS_ASSERT (kind == GetKind ());

  length = static_cast<uint32_t>(i.ReadU8 ());
  return length;
}

/////////////////////////////////////////////////////////
////////  TD_CAPABLE
/////////////////////////////////////////////////////////
TcpOptionTdTcpCapable::TcpOptionTdTcpCapable ()
  : TcpOptionTdTcp (),
    m_nsubflows(0)
{
  NS_LOG_FUNCTION (this);
}

TcpOptionTdTcpCapable::~TcpOptionTdTcpCapable ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

TypeId
TcpOptionTdTcpCapable::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpOptionTdTcpCapable")
    .SetParent<TcpOptionTdTcpMain> ()
    .AddConstructor<TcpOptionTdTcpCapable> ()
  ;
  return tid;
}

TypeId
TcpOptionTdTcpCapable::GetInstanceTypeId (void) const
{
  return TcpOptionTdTcpCapable::GetTypeId ();
}

bool
TcpOptionTdTcpCapable::operator== (const TcpOptionTdTcpCapable& opt) const
{
  return (m_nsubflows == opt.m_nsubflows);
}

void
TcpOptionTdTcpCapable::Print (std::ostream &os) const
{
  os << "TD_CAPABLE:"
     << " nsubflows=[" << (int)m_nsubflows << "]";
}

void
TcpOptionTdTcpCapable::Serialize (Buffer::Iterator i) const
{
  TcpOptionTdTcp::SerializeRef (i);

  i.WriteU8 ( (GetSubType () << 4) ); // Kind
  i.WriteU8 ( m_nsubflows ); //
}

uint32_t
TcpOptionTdTcpCapable::Deserialize (Buffer::Iterator i)
{
  uint32_t length = TcpOptionTdTcpMain::DeserializeRef (i);
  NS_ASSERT ( length == 4 );

  uint8_t subtype = i.ReadU8 ();
  NS_ASSERT ( subtype >> 4 == GetSubType () );

  SetNSubflows ( i.ReadU8 () );

  return length;
}

uint32_t
TcpOptionMpTcpCapable::GetSerializedSize (void) const
{
  return 4;
}


/////////////////////////////////////////////////////////
//// TD_DSS
/////////////////////////////////////////////////////////
TcpOptionTdTcpDSS::TcpOptionTdTcpDSS ()
  : TcpOptionTdTcp (),
    m_hasdata (false),
    m_hasack (false),
    m_dcarrier (0),
    m_acarrier (0),
    m_dsubflowid (0),
    m_asubflowid (0),
    m_dataseq (0),
    m_acknum (0)
{
  NS_LOG_FUNCTION (this);
}

TcpOptionTdTcpDSS::~TcpOptionTdTcpDSS ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
TcpOptionTdTcpDSS::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpOptionTdTcpDSS")
    .SetParent<TcpOptionTdTcpMain> ()
    .AddConstructor<TcpOptionTdTcpDSS> ()
  ;
  return tid;
}

TypeId
TcpOptionTdTcpDSS::GetInstanceTypeId (void) const
{
  return TcpOptionTdTcpDSS::GetTypeId ();
}

uint32_t
TcpOptionTdTcpDSS::GetSerializedSize (void) const
{
  return 16;
}

bool 
TcpOptionTdTcpDSS::operator== (const TcpOptionTdTcpDSS& other) const 
{
  return m_hasdata    == other.m_hasdata    &&
         m_hasack     == other.m_hasack     &&
         m_dsubflowid == other.m_dsubflowid &&
         m_asubflowid == other.m_asubflowid &&
         m_dcarrier   == other.m_dcarrier   &&
         m_acarrier   == other.m_acarrier   &&
         m_dataseq    == other.m_dataseq    &&
         m_acknum     == other.m_acknum;

}

void 
TcpOptionTdTcpDSS::SetData (const uint8_t& subflowId, const uint32_t & seq)
{
  NS_LOG_FUNCTION (this << subflowId << seq);
  m_dsubflowid = subflowId;
  m_dataseq = seq;
  m_hasdata = true;
}

void 
TcpOptionTdTcpDSS::SetAck (const uint8_t& subflowId, const uint32_t & ack) 
{
  NS_LOG_FUNCTION (this << subflowId << ack);
  m_asubflowid = subflowId;
  m_acknum = ack;
  m_hasack = true;
}

void 
TcpOptionTdTcpDSS::SetDataCarrier(const uint8_t &carrier)
{
  NS_LOG_FUNCTION (this << carrier);
  m_dcarrier = carrier;
}

void 
TcpOptionTdTcpDSS::SetAckCarrier(const uint8_t &carrier)
{
  NS_LOG_FUNCTION (this << carrier);
  m_acarrier = carrier;
}

bool 
TcpOptionTdTcpDSS::GetData (uint8_t& subflowId, uint32_t & seq)
{
  NS_LOG_FUNCTION (this);
  if (!m_hasdata)
    return false;

  subflowId = m_dsubflowid;
  seq = m_dataseq;
  return true;
}

bool 
TcpOptionTdTcpDSS::GetAck (uint8_t& subflowId, uint32_t & ack)
{
  NS_LOG_FUNCTION (this);

  if (!m_hasack)
    return false;

  subflowId = m_asubflowid;
  ack = m_acknum;
  return true;
}

bool 
TcpOptionTdTcpDSS::GetDataCarrier(uint8_t &carrier)
{
  NS_LOG_FUNCTION (this);
  if (!m_hasdata)
    return false;

  carrier = m_dcarrier;
  return true;
}

bool 
TcpOptionTdTcpDSS::GetAckCarrier(uint8_t &carrier)
{
  NS_LOG_FUNCTION (this);
  if (!m_hasack)
    return false;

  carrier = m_acarrier;
  return true;
}

//! Inherited
void 
TcpOptionTdTcpDSS::Print (std::ostream &os) const 
{
  os << "TD_DSS:"
     << " hasdata=[" << m_hasdata << "]"
     << " hasack=[" << m_hasack << "]";
  if (m_hasdata) {
    os << " dsubflow=[" << (int)m_dsubflowid << "]"
       << " sseq=[" << m_dataseq << "]";
  }
  if (m_hasack) {
    os << " asubflow=[" << (int)m_asubflowid << "]"
       << " sack=[" << m_acknum << "]";
  }

}

void 
TcpOptionTdTcpDSS::Serialize (Buffer::Iterator i) const
{
  NS_ASSERT (m_hasdata || m_hasack);

  TcpOptionTdTcp::SerializeRef (i);

  uint8_t subtypeAndFlag = (GetSubType () << 4) 
    | (m_hasdata ? (TD_DATA) : 0) | (m_hasack ? (TD_ACK) : 0);

  i.WriteU8(subtypeAndFlag); // Kind
  i.WriteU8(m_dsubflowid);
  i.WriteU8(m_dcarrier);
  i.WriteU8(m_asubflowid);
  i.WriteU8(m_acarrier);
  i.WriteHtonU32 ( m_dataseq );
  i.WriteHtonU32 ( m_acknum );

}

uint32_t 
TcpOptionTdTcpDSS::Deserialize (Buffer::Iterator i) 
{
  uint32_t length = TcpOptionTdTcpMain::DeserializeRef (i);
  NS_ASSERT (length == 16);

  uint8_t subtypeAndFlag = i.ReadU8();
  NS_ASSERT ( subtype >> 4 == GetSubType () );

  m_hasdata = subtypeAndFlag & TD_DATA;
  m_hasack = subtypeAndFlag & TD_ACK;

  uint8_t unused = i.ReadU8();

  m_dsubflowid = i.ReadU8();
  m_dcarrier   = i.ReadU8();
  m_asubflowid = i.ReadU8();
  m_acarrier   = i.ReadU8();

  m_dataseq = i.ReadNtohU32();
  m_acknum = i.ReadNtohU32();

  return length;
}

TcpOptionTdTcpDSS::TcpOptionTdTcpClose ()
  : TcpOptionTdTcp (),
    m_subflowid (0),
    m_lastseq (0)
{
  NS_LOG_FUNCTION (this);
}

TcpOptionTdTcpClose::~TcpOptionTdTcpClose ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
TcpOptionTdTcpClose::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpOptionTdTcpClose")
    .SetParent<TcpOptionTdTcpMain> ()
    .AddConstructor<TcpOptionTdTcpClose> ()
  ;
  return tid;
}

TypeId
TcpOptionTdTcpClose::GetInstanceTypeId (void) const
{
  return TcpOptionTdTcpClose::GetTypeId ();
}

uint32_t
TcpOptionTdTcpClose::GetSerializedSize (void) const
{
  return 8;
}

bool 
TcpOptionTdTcpClose::operator== (const TcpOptionTdTcpClose& other) const
{
  return (m_subflowid == other.subflowid) 
      && (m_lastseq == other.m_lastseq);
}


void 
TcpOptionTdTcpClose::SetSubflowLastSSeq(const uint8_t & subflowid, const uint32_t & lseq)
{
  NS_LOG_FUNCTION (this << subflowid << lseq);
  m_subflowid = subflowid;
  m_lastseq = lseq;
}

void 
TcpOptionTdTcpClose::GetSubflowLastSSeq(uint8_t & subflowid, uint32_t & lseq)
{
  NS_LOG_FUNCTION (this);
  subflowid = m_subflowid;
  lseq = m_lastseq;
}

void 
TcpOptionTdTcpClose::Print (std::ostream &os) const
{
  os << "TD_CLOSE:"
     << " subflowid=[" << m_subflowid << "]"
     << " lastseq=[" << m_lastseq << "]";
}

void 
TcpOptionTdTcpClose::Serialize (Buffer::Iterator i) const
{
  TcpOptionTdTcp::SerializeRef (i);

  uint8_t subtype = (GetSubType () << 4);

  i.WriteU8(subtype); // Kind
  i.WriteU8(m_subflowid);
  i.WriteHtonU32(m_lastseq);

}

uint32_t 
TcpOptionTdTcpClose::Deserialize (Buffer::Iterator i)
{
  uint32_t length = TcpOptionTdTcpMain::DeserializeRef (i);
  NS_ASSERT ( length == 8 );

  uint8_t subtype = i.ReadU8 ();
  NS_ASSERT ( subtype >> 4 == GetSubType () );

  uint8_t subflowid = i.ReadU8();
  uint32_t lastseq = i.ReadNtohU32();
  SetSubflowLastSSeq(subflowid, lastseq);

  return length;
}


}