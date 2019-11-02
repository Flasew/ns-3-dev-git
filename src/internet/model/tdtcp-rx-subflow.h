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

#ifndef TDTCP_RX_SUBFLOW_H
#define TDTCP_RX_SUBFLOW_H

#include "ns3/trace-source-accessor.h"
#include "ns3/sequence-number.h"
#include "ns3/rtt-estimator.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"
#include "ns3/tcp-socket.h"
#include "ns3/ipv4-end-point.h"
#include "ns3/ipv4-address.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-header.h"
#include "tdtcp-mapping.h"
#include "tdtcp-socket-base.h"

namespace ns3 {

class TdTcpRxSubflow {

public:
  TdTcpRxSubflow(uint8_t id, Ptr<TdTcpSocketBase> tdtcp);
  ~TdTcpRxSubflow();

  void ReceivedData (Ptr<Packet> packet, 
                     const TcpHeader& tcpHeader, 
                     SequenceNumber32 sseq, 
                     uint8_t scid);

  Ptr<Packet> ExtractAtMostOneMapping(uint32_t maxSize,
                                      bool only_full_mapping, 
                                      SequenceNumber32& headDSN);
private:
  Ptr<TdTcpSocketBase> m_meta;
  Ptr<TcpRxBuffer> m_rxBuffer;
  uint8_t m_subflowid;
  TdTcpMappingContainer m_RxMappings;  //!< List of mappings to receive

};


}

#endif // TDTCP_RX_SUBFLOW_H