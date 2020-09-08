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

#include <algorithm>
#include <iostream>
#include "ns3/tdtcp-rx-subflow.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/tdtcp-socket-base.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-end-point.h"
#include "ns3/node.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/trace-helper.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TdTcpRxSubflow");

static inline TdTcpMapping
GetMapping(uint32_t dseq, uint32_t sseq, uint16_t length)
{
  TdTcpMapping mapping;
  mapping.SetHeadDSN(SequenceNumber32(dseq));
  mapping.SetMappingSize(length);
  mapping.MapToSSN(SequenceNumber32(sseq));
  return mapping;
}

TypeId 
TdTcpRxSubflow::GetdTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TdTcpRxSubflow")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
    .AddConstructor<TdTcpRxSubflow> ()
  ;
  return tid;
}

TypeId 
TdTcpRxSubflow::GetInstanceTypeId ()
{
  return TdTcpRxSubflow::GetTypeId();
}

TdTcpRxSubflow::TdTcpRxSubflow(uint8_t id, Ptr<TdTcpSocketBase> tdtcp)
{
  m_meta = tdtcp;
  m_subflowid = id;
  m_rxBuffer = CreateObject<TcpRxBuffer> ();
  m_rxBuffer->SetMaxBufferSize(m_meta->m_rxBuffer->MaxBufferSize());
}

TdTcpRxSubflow::~TdTcpRxSubflow()
{

}


void 
TdTcpRxSubflow::ReceivedData (Ptr<Packet> p, const TcpHeader& tcpHeader, SequenceNumber32 sseq, uint8_t scid)
{
  NS_LOG_FUNCTION (this << p << tcpHeader << sseq << scid);
  TdTcpMapping mapping;

  // OutOfRange
  // If cannot find an adequate mapping, then it should [check RFC]
  if(!m_RxMappings.GetMappingForSSN(sseq, mapping))
  {
   m_RxMappings.Dump();
   NS_FATAL_ERROR("Could not find mapping associated ");
   return;
  }
  // Put into Rx buffer
  SequenceNumber32 expectedSSN = m_rxBuffer->NextRxSequence();
  if (!m_rxBuffer->Add(p, sseq))
  { // Insert failed: No data or RX buffer full
    NS_LOG_WARN("Insert failed, No data (" << p->GetSize() << ") ?");

    m_rxBuffer->Dump();
    m_meta->SendAckPacket(m_subflowid, scid,
                          m_rxBuffer->NextRxSequence().GetValue());
    return;
  }

  // Notify app to receive if necessary
  if (expectedSSN < m_rxBuffer->NextRxSequence())
  { // NextRxSeq advanced, we have something to send to the app
    m_meta->OnSubflowReceive(p, tcpHeader, this, m_rxBuffer->NextRxSequence().GetValue());
  }
  m_meta->SendAckPacket(m_subflowid, scid,
                        m_rxBuffer->NextRxSequence().GetValue());

}

Ptr<Packet>
TdTcpRxSubflow::ExtractAtMostOneMapping(uint32_t maxSize, bool only_full_mapping, SequenceNumber32& headDSN)
{
  TdTcpMapping mapping;
  Ptr<Packet> p = Create<Packet>();
  uint32_t rxAvailable = m_rxBuffer->Available (); 
  if(rxAvailable == 0)
  {
    NS_LOG_LOGIC("Nothing to extract");
    return p;
  }
  else
  {
    NS_LOG_LOGIC(rxAvailable  << " Rx available");
  }

  // as in linux, we extract in order
  SequenceNumber32 headSSN = m_rxBuffer->HeadSequence();
  if(!m_RxMappings.GetMappingForSSN(headSSN, mapping))
  {
    m_RxMappings.Dump();
    NS_FATAL_ERROR("Could not associate a mapping to ssn [" << headSSN << "]. Should be impossible");
  }
  headDSN = mapping.HeadDSN();

  if(only_full_mapping) 
  {

    if(mapping.GetLength() > maxSize)
    {
      NS_LOG_DEBUG("Not enough space available to extract the full mapping");
      return p;
    }
    if(m_rxBuffer->Available() < mapping.GetLength())
    {
      NS_LOG_DEBUG("Mapping not fully received yet");
      return p;
    }
  }

  // Extract at most one mapping
  maxSize = std::min(maxSize, (uint32_t)mapping.GetLength());
  NS_LOG_DEBUG("Extracting at most " << maxSize << " bytes ");
  p = m_rxBuffer->Extract( maxSize );
  SequenceNumber32 extractedTail = headSSN + p->GetSize() - 1;
  NS_ASSERT_MSG( extractedTail <= mapping.TailSSN(), "Can not extract more than the size of the mapping");

  if(extractedTail < mapping.TailSSN() )
  {
    NS_ASSERT_MSG(!only_full_mapping, "The only extracted size possible should be the one of the mapping");
    // only if data extracted covers full mapping we can remove the mapping
  }
  else
  {
    m_RxMappings.DiscardMapping(mapping);
  }
  return p;
}

}