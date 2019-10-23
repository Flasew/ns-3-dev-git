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

#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/ipv6.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv6-route.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/simulation-singleton.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/pointer.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/data-rate.h"
#include "ns3/object.h"
#include "tcp-socket-base.h"
#include "tcp-l4-protocol.h"
#include "ipv4-end-point.h"
#include "ipv6-end-point.h"
#include "ipv6-l3-protocol.h"
#include "tcp-tx-buffer.h"
#include "tcp-rx-buffer.h"
#include "rtt-estimator.h"
#include "tcp-header.h"
#include "tcp-option-winscale.h"
#include "tcp-option-ts.h"
#include "tcp-option-sack-permitted.h"
#include "tcp-option-sack.h"
#include "tcp-congestion-ops.h"
#include "tcp-recovery-ops.h"
#include "mptcp-crypto.h"
#include "mptcp-subflow.h"
#include "mptcp-socket-base.h"
#include "tcp-option-mptcp.h"
#include "ns3/tdtcp-socket-base.h"

#include <math.h>
#include <algorithm>


namespace ns3 {

NS_LOG_COMPONENT_DEFINE("TdTcpSocketBase");

NS_OBJECT_ENSURE_REGISTERED(TdTcpSocketBase); 

TypeId
TdTcpSocketBase::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TdTcpSocketBase")
    .SetParent<TcpSocketBase> ()
    .SetGroupName ("Internet")
    .AddConstructor<TdTcpSocketBase> ()
  ;
  return tid;
}

TypeId
TdTcpSocketBase::GetInstanceTypeId () const
{
  return TdTcpSocketBase::GetTypeId ();
}

TdTcpSocketBase::TdTcpSocketBase() 
  : TcpSocketBase(),
    m_currTxSubflow(0)
{
  NS_LOG_FUNCTION(this);
}

TdTcpSocketBase::TdTcpSocketBase(const TcpSocketBase& sock)
  : TcpSocketBase(sock),
    m_currTxSubflow(0)
{
  NS_LOG_FUNCTION(this << sock);
}

TdTcpSocketBase::TdTcpSocketBase(const TdTcpSocketBase& sock)
  : TcpSocketBase(sock),
    m_txsubflows(sock.m_txsubflows),
    m_rxsubflows(sock.m_rxsubflows),
    m_currTxSubflow(0)
{
  NS_LOG_FUNCTION(this << sock);
}

TdTcpSocketBase::~TdTcpSocketBase() 
{

}

int TdTcpSocketBase::SetupCallback (void) 
{
  TcpSocketBase::SetupCallback();
}

int TdTcpSocketBase::DoConnect (void) 
{
  TcpSocketBase::DoConnect();
}

int TdTcpSocketBase::DoClose (void) 
{
  TcpSocketBase::DoClose();
}

void TdTcpSocketBase::CloseAndNotify (void) 
{
  TcpSocketBase::CloseAndNotify();
}

// Whether something is in range of the rx buffer should be left
// for the rx_subflows to decide... 
bool
TdTcpSocketBase::OutOfRange (SequenceNumber32 head, SequenceNumber32 tail) const
{
  // if (m_state == LISTEN || m_state == SYN_SENT || m_state == SYN_RCVD)
  //   { // Rx buffer in these states are not initialized.
  //     return false;
  //   }
  // if (m_state == LAST_ACK || m_state == CLOSING || m_state == CLOSE_WAIT)
  //   { // In LAST_ACK and CLOSING states, it only wait for an ACK and the
  //     // sequence number must equals to m_rxBuffer->NextRxSequence ()
  //     return (m_rxBuffer->NextRxSequence () != head);
  //   }

  // // In all other cases, check if the sequence number is in range
  // return (tail < m_rxBuffer->NextRxSequence () || m_rxBuffer->MaxRxSequence () <= head);
  NS_LOG_DEBUG ("OutOfRange shouldn't be called for TdTcp");
  return false;
}

/* Override DoForwardUp for these functions */
void
TdTcpSocketBase::ForwardUp (Ptr<Packet> packet, Ipv4Header header, uint16_t port,
                          Ptr<Ipv4Interface> incomingInterface)
{
  TcpSocketBase::ForwardUp(packet, header, port, incomingInterface);
}

void
TdTcpSocketBase::ForwardUp6 (Ptr<Packet> packet, Ipv6Header header, uint16_t port,
                           Ptr<Ipv6Interface> incomingInterface)
{
  TcpSocketBase::ForwardUp6(packet, header, port, incomingInterface);
}

// These will become important in the future for indicating network change
// For now just leave as-is...
void
TcpSocketBase::ForwardIcmp (Ipv4Address icmpSource, uint8_t icmpTtl,
                            uint8_t icmpType, uint8_t icmpCode,
                            uint32_t icmpInfo)
{
  NS_LOG_FUNCTION (this << icmpSource << static_cast<uint32_t> (icmpTtl) <<
                   static_cast<uint32_t> (icmpType) <<
                   static_cast<uint32_t> (icmpCode) << icmpInfo);
  TcpSocketBase::ForwardIcmp(icmpSource, icmpTtl, icmpType, icmpCode, icmpInfo);
}

void
TcpSocketBase::ForwardIcmp6 (Ipv6Address icmpSource, uint8_t icmpTtl,
                             uint8_t icmpType, uint8_t icmpCode,
                             uint32_t icmpInfo)
{
  NS_LOG_FUNCTION (this << icmpSource << static_cast<uint32_t> (icmpTtl) <<
                   static_cast<uint32_t> (icmpType) <<
                   static_cast<uint32_t> (icmpCode) << icmpInfo);
  TcpSocketBase::ForwardIcmp6(icmpSource, icmpTtl, icmpType, icmpCode, icmpInfo);
}

// Received packet from L3
// matches the packet to the proper rxsubflow 

void
TdTcpSocketBase::DoForwardUp (Ptr<Packet> packet, const Address &fromAddress,
                            const Address &toAddress) 
{
  // in case the packet still has a priority tag attached, remove it
  SocketPriorityTag priorityTag;
  packet->RemovePacketTag (priorityTag);

  // Peel off TCP header
  TcpHeader tcpHeader;
  packet->RemoveHeader (tcpHeader);
  SequenceNumber32 seq = tcpHeader.GetSequenceNumber ();

  m_rxTrace (packet, tcpHeader, this);

  if (tcpHeader.GetFlags () & TcpHeader::SYN) {
    /* The window field in a segment where the SYN bit is set (i.e., a <SYN>
     * or <SYN,ACK>) MUST NOT be scaled (from RFC 7323 page 9). But should be
     * saved anyway..
     */
    m_rWnd = tcpHeader.GetWindowSize ();

    if (tcpHeader.HasOption (TcpOption::WINSCALE) && m_winScalingEnabled) {
      ProcessOptionWScale (tcpHeader.GetOption (TcpOption::WINSCALE));
    }
    else {
      m_winScalingEnabled = false;
    }

    if (tcpHeader.HasOption (TcpOption::SACKPERMITTED) && m_sackEnabled) {
      ProcessOptionSackPermitted (tcpHeader.GetOption (TcpOption::SACKPERMITTED));
    }
    else {
      m_sackEnabled = false;
    }

    // When receiving a <SYN> or <SYN-ACK> we should adapt TS to the other end
    if (tcpHeader.HasOption (TcpOption::TS) && m_timestampEnabled) {
      ProcessOptionTimestamp (tcpHeader.GetOption (TcpOption::TS),
                               tcpHeader.GetSequenceNumber ());
    }
    else {
      m_timestampEnabled = false;
    }

    if (tcpHeader.HasOption (TcpOption::TDTCP) && m_tdtcpEnabled) {
      ProcessOptionTdTcp(tcpHeader.GetOption(TcpOption::TDTCP));
    }
    else {
      NS_FATAL_ERROR("Don't use TDTCP socket for regular TCP for now...");
    }

    // Initialize cWnd and ssThresh
    m_tcb->m_cWnd = GetInitialCwnd () * GetSegSize ();
    m_tcb->m_cWndInfl = m_tcb->m_cWnd;
    m_tcb->m_ssThresh = GetInitialSSThresh ();

    if (tcpHeader.GetFlags () & TcpHeader::ACK) {
        EstimateRtt (tcpHeader);
        m_highRxAckMark = tcpHeader.GetAckNumber ();
    }
  }
}

}



















