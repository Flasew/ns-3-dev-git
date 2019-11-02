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
#ifndef TDTCP_TX_SUBFLOW_H
#define TDTCP_TX_SUBFLOW_H

#include <stdint.h>
#include <queue>
#include "ns3/traced-value.h"
#include "ns3/tcp-socket.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv6-header.h"
#include "ns3/timer.h"
#include "ns3/sequence-number.h"
#include "ns3/data-rate.h"
#include "ns3/node.h"
#include "ns3/tcp-socket-state.h"
#include "ns3/ipv4-end-point.h"
#include "tcp-tx-buffer.h"
#include "ns3/tdtcp-mapping.h"

namespace ns3 {

class TdTcpTxSubflow {

  friend class TdTcpSocketBase;
  friend class TdTcpRxSubflow;

public:
  TdTcpTxSubflow (uint8_t id, Ptr<TdTcpSocketBase> tdtcp);
  ~TdTcpTxSubflow();

  // Send data
  uint32_t SendDataPacket (SequenceNumber32 seq,  
                           uint32_t maxSize, 
                           bool withAck);


  // Received Ack relevant
  void ReceivedAck(Ptr<Packet>, const TcpHeader&); // Received an ACK packet
  void ProcessAck (const SequenceNumber32 &ackNumber, 
                  const SequenceNumber32 &oldHeadSequence);
  void EnterRecovery ();
  void DupAck ();
  void DoRetransmit ();
  void NewAck (SequenceNumber32 const& ack, bool resetRTO);

  uint32_t AvailableWindow () const;
  uint32_t Window (void) const;

private:

  uint8_t m_subflowid;

  Ptr<TdTcpSocketBase> m_meta;
  Ptr<RttEstimator> m_rtt; //!< Round trip time estimator

  Ptr<TcpTxBuffer> m_txBuffer; //!< Tx buffer

  // Fast Retransmit and Recovery
  SequenceNumber32       m_recover    {0};   //!< Previous highest Tx seqnum for fast recovery (set it to initial seq number)
  uint32_t               m_retxThresh {3};   //!< Fast Retransmit threshold
  bool                   m_limitedTx  {true}; //!< perform limited transmit

  // Transmission Control Block
  Ptr<TcpSocketState>    m_tcb;               //!< Congestion control information
  Ptr<TcpCongestionOps>  m_congestionControl; //!< Congestion control
  Ptr<TcpRecoveryOps>    m_recoveryOps;       //!< Recovery Algorithm

  // Pacing related variable
  Timer m_pacingTimer {Timer::REMOVE_ON_DESTROY}; //!< Pacing Event

  Ptr<TcpTxBuffer> m_txBuffer;
  TdTcpMappingContainer m_TxMappings;  //!< List of mappings to send
};


}

#endif // TDTCP_TX_SUBFLOW_H