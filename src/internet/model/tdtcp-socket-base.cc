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
#include "tdtcp-socket-base.h"
#include "tdtcp-tx-subflow.h"
#include "tdtcp-rx-subflow.h"
#include "tdtcp-mapping.h"

#include <math.h>
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("TdTcpSocketBase");

NS_OBJECT_ENSURE_REGISTERED(TdTcpSocketBase); 

static inline TdTcpMapping
GetMapping(uint32_t dseq, uint32_t sseq, uint16_t length)
{
  TdTcpMapping mapping;
  mapping.SetHeadDSN(dseq);
  mapping.SetMappingSize(length);
  mapping.MapToSSN(sseq);
  return mapping;
}

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
    m_currTxSubflow(0),
    m_connDupAckTh(sock.m_connDupAckTh),
    m_seqToSubflowMap(sock.m_seqToSubflowMap)
{
  NS_LOG_FUNCTION(this << sock);
}

TdTcpSocketBase::~TdTcpSocketBase() 
{

}

int 
TdTcpSocketBase::SetupCallback (void) 
{
  TcpSocketBase::SetupCallback();
}

// Whether something is in range of the rx buffer should be left
// for the rx_subflows to decide... 
bool
TdTcpSocketBase::OutOfRange (SequenceNumber32 head, SequenceNumber32 tail) const
{

  NS_LOG_DEBUG ("OutOfRange shouldn't be called for TdTcp");
  return false;
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

int
TdTcpSubflow::ProcessOptionTdTcp (const Ptr<const TcpOption> option)
{
  //! adds the header
  NS_LOG_FUNCTION(option);
  Ptr<const TcpOptionTdTcpMain> main = DynamicCast<const TcpOptionTdTcpMain>(option);
  switch(main->GetSubType())
     {
       case TcpOptionTdTcpMain::TD_CAPABLE:
            return ProcessOptionTdTcpCapable(main);
       case TcpOptionTdTcpMain::TD_DSS:
            {
              Ptr<const TcpOptionTdTcpDSS> dss = DynamicCast<const TcpOptionTdTcpDSS>(option);
              NS_ASSERT(dss);
              // Update later on
              ProcessOptionTdTcpDSSEstablished(dss);
            }
            break;
       case TcpOptionMpTcpMain::TD_CLOSE:
       default:
            NS_FATAL_ERROR("Unsupported yet");
            break;
      };
  return 0;
}

void 
TdTcpSocketBase::CompleteFork (Ptr<Packet> packet, const TcpHeader& tcpHeader,
                      const Address& fromAddress, const Address& toAddress) 
{
  NS_LOG_FUNCTION (this << p << h << fromAddress << toAddress);
  NS_UNUSED (p);
  // Get port and address from peer (connecting host)
  if (InetSocketAddress::IsMatchingType (toAddress))
  {
    m_endPoint = m_tcp->Allocate (GetBoundNetDevice (),
                                  InetSocketAddress::ConvertFrom (toAddress).GetIpv4 (),
                                  InetSocketAddress::ConvertFrom (toAddress).GetPort (),
                                  InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 (),
                                  InetSocketAddress::ConvertFrom (fromAddress).GetPort ());
    m_endPoint6 = nullptr;
  }
  else if (Inet6SocketAddress::IsMatchingType (toAddress))
  {
    m_endPoint6 = m_tcp->Allocate6 (GetBoundNetDevice (),
                                    Inet6SocketAddress::ConvertFrom (toAddress).GetIpv6 (),
                                    Inet6SocketAddress::ConvertFrom (toAddress).GetPort (),
                                    Inet6SocketAddress::ConvertFrom (fromAddress).GetIpv6 (),
                                    Inet6SocketAddress::ConvertFrom (fromAddress).GetPort ());
    m_endPoint = nullptr;
  }
  m_tcp->AddSocket (this);

  // Change the cloned socket from LISTEN state to SYN_RCVD
  NS_LOG_DEBUG ("LISTEN -> SYN_RCVD");
  m_state = SYN_RCVD;
  m_synCount = m_synRetries;
  m_dataRetrCount = m_dataRetries;
  SetupCallback ();
  // Set the sequence number and send SYN+ACK
  m_rxBuffer->SetNextRxSequence (h.GetSequenceNumber () + SequenceNumber32 (1));

  // TDTCP : extract number of sender flows from SYN packet
  uint8_t peernsubflow = ProcessOptionTdTcpCapable (tcpHeader.GetOption(TcpOption::TDTCP));
  NS_ASSERT(peernsubflow > 0);

  for (uint8_t i = 0; i < peernsubflow; i++) {
    m_rxsubflows.emplace_back(i);
  }

  // now crete the SYN/ACK packet
  SendSYN(true);

}

void 
TdTcpSocketBase::SendSYN (bool withAck)
{
  NS_LOG_FUNCTION(this);

  if (m_endPoint == nullptr && m_endPoint6 == nullptr)
  {
    NS_LOG_WARN ("Failed to send empty packet due to null endpoint");
    return;
  }

  Ptr<Packet> p = Create<Packet> ();
  TcpHeader header;
  SequenceNumber32 s = m_tcb->m_nextTxSequence;

  flags |= (TcpHeader::SYN);
  if (withAck)
    flags |= (TcpHeader::ACK);

  AddSocketTags (p);

  header.SetFlags (flags);
  header.SetSequenceNumber (s);
  header.SetAckNumber (m_rxBuffer->NextRxSequence ());
  if (m_endPoint != nullptr)
  {
    header.SetSourcePort (m_endPoint->GetLocalPort ());
    header.SetDestinationPort (m_endPoint->GetPeerPort ());
  }
  else
  {
    header.SetSourcePort (m_endPoint6->GetLocalPort ());
    header.SetDestinationPort (m_endPoint6->GetPeerPort ());
  }

  AddOptionTDSYN(header, m_tdNSubflows);

  if (m_winScalingEnabled)
  { // The window scaling option is set only on SYN packets
    AddOptionWScale (header);
  }

  // SACK is not allowed for now.
  // if (m_sackEnabled)
  // {
  //   AddOptionSackPermitted (header);
  // }

  if (m_synCount == 0)
  { // No more connection retries, give up
    NS_LOG_LOGIC ("Connection failed.");
    m_rtt->Reset (); //According to recommendation -> RFC 6298
    CloseAndNotify ();
    return;
  }
  else
  { // Exponential backoff of connection time out
    int backoffCount = 0x1 << (m_synRetries - m_synCount);
    m_rto = m_cnTimeout * backoffCount;
    m_synCount--;
  }

  windowSize = AdvertisedWindowSize (false);
  m_txTrace (p, header, this);

  if (m_endPoint != nullptr)
  {
    m_tcp->SendPacket (p, header, m_endPoint->GetLocalAddress (),
                       m_endPoint->GetPeerAddress (), m_boundnetdevice);
  }
  else
  {
    m_tcp->SendPacket (p, header, m_endPoint6->GetLocalAddress (),
                       m_endPoint6->GetPeerAddress (), m_boundnetdevice);
  }

  if (m_retxEvent.IsExpired ())
  { // Retransmit SYN / SYN+ACK / FIN / FIN+ACK to guard against lost
    NS_LOG_LOGIC ("Schedule retransmission timeout at time "
                  << Simulator::Now ().GetSeconds () << " to expire at time "
                  << (Simulator::Now () + m_rto.Get ()).GetSeconds ());
    m_retxEvent = Simulator::Schedule (m_rto, &TdTcpSocketBase::SendSYN, this, withAck);
  }
}

int 
TdTcpSocketBase::DoConnect (void) 
{
  NS_LOG_FUNCTION (this);

  if (m_state == CLOSED || m_state == LISTEN || m_state == SYN_SENT || m_state == LAST_ACK || m_state == CLOSE_WAIT)
  { // send a SYN packet and change state into SYN_SENT
    // send a SYN packet with ECE and CWR flags set if sender is ECN capable
    SendSYN();
    NS_LOG_DEBUG (TcpStateName[m_state] << " -> SYN_SENT");
    m_state = SYN_SENT;
  }
  else if (m_state != TIME_WAIT)
  { // In states SYN_RCVD, ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, and CLOSING, an connection
    // exists. We send RST, tear down everything, and close this socket.
    SendRST ();
    CloseAndNotify ();
  }
  return 0;

}

uint8_t 
TdTcpSocketBase::ProcessOptionTdTcpCapable (Ptr<const TcpOption> opt) {
  
  if (!opt)
  {
    NS_LOG_WARN ("ProcessOption TDSYN received null pointer");
    return 0;
  }

  Ptr<const TdTcpOptionTdCapable> tdc = DynamicCast<const TcpOptionTdTcpCapable>(opt);
  if (!tdc)
  {
    NS_LOG_WARN ("ProcessOption TDSYN some nonsense");
    return 0;
  }

  return tdc->GetNSubflows();
}

void 
TdTcpSocketBase:: AddOptionTDSYN(TcpHeader &header, uint8_t n) 
{ 
  NS_LOG_FUNCTION(this << header << n);
  NS_ASSERT (header.GetFlags () & TcpHeader::SYN);

  Ptr<TcpOptionTdTcpCapable> option = CreateObject<TcpOptionTdTcpCapable>();
  option->SetNSubflows(n);
  header.AppendOption(option);

  NS_LOG_INFO (m_node->GetId() << " set a TDCapable with nsubflows=" << (int)n);

}

void 
TdTcpSocketBase:: AddOptionTDDSS(TcpHeader &header, 
                                bool data,
                                uint8_t ssid,
                                uint8_t csid, 
                                uint32_t sseq, 
                                bool ack, 
                                uint8_t said, 
                                uint8_t caid, 
                                uint32_t sack) 
{ 
  NS_LOG_FUNCTION(this << header << data << ssid << csid << sseq << ack << said << caid << sack);

  Ptr<TcpOptionTdTcpCapable> option = CreateObject<TcpOptionTdTcpDSS>();
  option->SetNSubflows(n);

  if (data)
  {
    option->SetData(ssid, sseq);
    option->SetDataCarrier(csid);
  }

  if (ack)
  {
    option->SetAck(said, sack);
    option->SetAckCarrier(caid);
  }

  header.AppendOption(option);
}

void 
TdTcpSocketBase::ProcessSynSent (Ptr<Packet> packet, const TcpHeader& tcpHeader) 
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Extract the flags. PSH and URG are disregarded.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG);

  if (tcpflags == 0)
  { // Bare data, this is wrong and ignore it
  }
  else if (tcpflags & TcpHeader::ACK && !(tcpflags & TcpHeader::SYN))
  { // Ignore ACK in SYN_SENT
  }
  else if (tcpflags & TcpHeader::SYN && !(tcpflags & TcpHeader::ACK))
  { // Received SYN, move to SYN_RCVD state and respond with SYN+ACK
    NS_LOG_DEBUG ("SYN_SENT -> SYN_RCVD");
    m_state = SYN_RCVD;
    m_synCount = m_synRetries;
    m_rxBuffer->SetNextRxSequence (tcpHeader.GetSequenceNumber () + SequenceNumber32 (1));

    m_tcb->m_ecnState = TcpSocketState::ECN_DISABLED;
    SendSYN (true);

  }

  else if (tcpflags & (TcpHeader::SYN | TcpHeader::ACK)
           && m_tcb->m_nextTxSequence + SequenceNumber32 (1) == tcpHeader.GetAckNumber ())
  { // Handshake completed
    uint8_t peernsubflows = ProcessOptionTdTcpCapable (tcpHeader.GetOption(TcpOption::TDTCP));
    NS_ASSERT (peernsubflows > 0);

    for (uint8_t i = 0; i < peernsubflow; i++) {
      m_rxsubflows.emplace_back(i);
    }

    for (uint8_t i = 0; i < m_tdNSubflows; i++) {
      m_txsubflows.emplace_back(i);
    }

    NS_LOG_DEBUG ("SYN_SENT -> ESTABLISHED");
    // m_congestionControl->CongestionStateSet (m_tcb, TcpSocketState::CA_OPEN);
    m_state = ESTABLISHED;
    m_connected = true;
    m_retxEvent.Cancel ();
    m_rxBuffer->SetNextRxSequence (tcpHeader.GetSequenceNumber () + SequenceNumber32 (1));
    m_tcb->m_highTxMark = ++m_tcb->m_nextTxSequence;
    m_txBuffer->SetHeadSequence (m_tcb->m_nextTxSequence);
    SendHandShakeACK();

    m_tcb->m_ecnState = TcpSocketState::ECN_DISABLED;

    SendPendingData (m_connected);
    Simulator::ScheduleNow (&TdTcpSocketBase::ConnectionSucceeded, this);
    // Always respond to first data packet to speed up the connection.
    // Remove to get the behaviour of old NS-3 code.
    m_delAckCount = m_delAckMaxCount;
  }
  else
  { // Other in-sequence input
    if (!(tcpflags & TcpHeader::RST))
    { // When (1) rx of FIN+ACK; (2) rx of FIN; (3) rx of bad flags
      NS_LOG_LOGIC ("Illegal flag combination " << TcpHeader::FlagsToString (tcpHeader.GetFlags ()) <<
                    " received in SYN_SENT. Reset packet is sent.");
      SendRST ();
    }
    CloseAndNotify ();
  }
}

void 
TdTcpSocketBase::SendHandShakeACK ()
{
  NS_LOG_FUNCTION(this);

  if (m_endPoint == nullptr && m_endPoint6 == nullptr)
  {
    NS_LOG_WARN ("Failed to send empty packet due to null endpoint");
    return;
  }

  Ptr<Packet> p = Create<Packet> ();
  TcpHeader header;
  SequenceNumber32 s = m_tcb->m_nextTxSequence;

  flags |= (TcpHeader::ACK);

  AddSocketTags (p);

  header.SetFlags (flags);
  header.SetSequenceNumber (s);
  header.SetAckNumber (m_rxBuffer->NextRxSequence ());
  if (m_endPoint != nullptr)
  {
    header.SetSourcePort (m_endPoint->GetLocalPort ());
    header.SetDestinationPort (m_endPoint->GetPeerPort ());
  }
  else
  {
    header.SetSourcePort (m_endPoint6->GetLocalPort ());
    header.SetDestinationPort (m_endPoint6->GetPeerPort ());
  }

  AddOptionTDSYN(header, m_tdNSubflows);

  windowSize = AdvertisedWindowSize (false);
  m_txTrace (p, header, this);

  if (m_endPoint != nullptr)
  {
    m_tcp->SendPacket (p, header, m_endPoint->GetLocalAddress (),
                       m_endPoint->GetPeerAddress (), m_boundnetdevice);
  }
  else
  {
    m_tcp->SendPacket (p, header, m_endPoint6->GetLocalAddress (),
                       m_endPoint6->GetPeerAddress (), m_boundnetdevice);
  }

  if (m_retxEvent.IsExpired ())
  { // Retransmit SYN / SYN+ACK / FIN / FIN+ACK to guard against lost
    NS_LOG_LOGIC ("Schedule retransmission timeout at time "
                  << Simulator::Now ().GetSeconds () << " to expire at time "
                  << (Simulator::Now () + m_rto.Get ()).GetSeconds ());
    m_retxEvent = Simulator::Schedule (m_rto, &TdTcpSocketBase::SendSYN, this, withAck);
  }
}

void 
TdTcpSocketBase::ProcessSynRcvd (Ptr<Packet> packet, const TcpHeader& tcpHeader,
                       const Address& fromAddress, const Address& toAddress)
{
  NS_UNUSED (toAddress);
  NS_LOG_FUNCTION (this << tcpHeader);

  // Extract the flags. PSH, URG, CWR and ECE are disregarded.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG | TcpHeader::CWR | TcpHeader::ECE);
  // Ptr<const TcpOptionTdTcpCapable> cap;

  if ((tcpflags == 0
        || ((tcpflags == TcpHeader::ACK
                  && m_tcb->m_nextTxSequence + SequenceNumber32 (1) == tcpHeader.GetAckNumber ())))
      )
  { // If it is bare data, accept it and move to ESTABLISHED state. This is
    // possibly due to ACK lost in 3WHS. If in-sequence ACK is received, the
    // handshake is completed nicely.
    NS_LOG_DEBUG ("SYN_RCVD -> ESTABLISHED");
    m_congestionControl->CongestionStateSet (m_tcb, TcpSocketState::CA_OPEN);
    m_state = ESTABLISHED;
    m_connected = true;
    m_retxEvent.Cancel ();
    m_tcb->m_highTxMark = ++m_tcb->m_nextTxSequence;
    m_txBuffer->SetHeadSequence (m_tcb->m_nextTxSequence);

    for (uint8_t i = 0; i < m_tdNSubflows; i++) {
      m_txsubflows.emplace_back(i);
    }

    if (m_endPoint)
    {
      m_endPoint->SetPeer (InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 (),
                           InetSocketAddress::ConvertFrom (fromAddress).GetPort ());
    }
    else if (m_endPoint6)
    {
      m_endPoint6->SetPeer (Inet6SocketAddress::ConvertFrom (fromAddress).GetIpv6 (),
                            Inet6SocketAddress::ConvertFrom (fromAddress).GetPort ());
    }

    NotifyNewConnectionCreated (this, fromAddress);

    Ptr<const TcpOptionTdTcpCapable> cap;
    if (GetTcpOption(tcpHeader, cap)) {
      // merely a handshake packet
      
    }
    else 
    {
      // If it's somehow a data packet... 
      // Assume it's just a packet in Established state.
      ProcessEstablished (packet, tcpHeader)
    }
      
    // As this connection is established, the socket is available to send data now
    if (GetTxAvailable () > 0)
    {
      NotifySend (GetTxAvailable ());
    }
  }
  else if (tcpflags == TcpHeader::SYN)
  { // Probably the peer lost my SYN+ACK
    m_rxBuffer->SetNextRxSequence (tcpHeader.GetSequenceNumber () + SequenceNumber32 (1));
    m_tcb->m_ecnState = TcpSocketState::ECN_DISABLED;
    SendSYN (true);

  }

  // Connection close is todo...
  else if (tcpflags == (TcpHeader::FIN | TcpHeader::ACK))
  {
    //TODO 
    if (tcpHeader.GetSequenceNumber () == m_rxBuffer->NextRxSequence ())
    { // In-sequence FIN before connection complete. Set up connection and close.
      m_connected = true;
      m_retxEvent.Cancel ();
      m_tcb->m_highTxMark = ++m_tcb->m_nextTxSequence;
      m_txBuffer->SetHeadSequence (m_tcb->m_nextTxSequence);
      if (m_endPoint)
      {
        m_endPoint->SetPeer (InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 (),
                             InetSocketAddress::ConvertFrom (fromAddress).GetPort ());
      }
      else if (m_endPoint6)
      {
        m_endPoint6->SetPeer (Inet6SocketAddress::ConvertFrom (fromAddress).GetIpv6 (),
                              Inet6SocketAddress::ConvertFrom (fromAddress).GetPort ());
      }
      NotifyNewConnectionCreated (this, fromAddress);
      PeerClose (packet, tcpHeader);
    }
  }

  else
  { // Other in-sequence input
    if (tcpflags != TcpHeader::RST)
    { // When (1) rx of SYN+ACK; (2) rx of FIN; (3) rx of bad flags
      NS_LOG_LOGIC ("Illegal flag " << TcpHeader::FlagsToString (tcpflags) <<
                    " received. Reset packet is sent.");
      if (m_endPoint)
      {
        m_endPoint->SetPeer (InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 (),
                             InetSocketAddress::ConvertFrom (fromAddress).GetPort ());
      }
      else if (m_endPoint6)
      {
        m_endPoint6->SetPeer (Inet6SocketAddress::ConvertFrom (fromAddress).GetIpv6 (),
                              Inet6SocketAddress::ConvertFrom (fromAddress).GetPort ());
      }
      SendRST ();
    }
    CloseAndNotify ();
  }
}

// ESTABLISHED: SEND/RECEIVE DATA
bool 
TdTcpSocketBase::ProcessOptionTdDSS (const Ptr<const TcpOption> & option, 
                         int16_t & ssubflow,
                         uint32_t & sseq,
                         int16_t & rsubflow,
                         uint32_t & sack) 
{
  if (!opt)
  {
    NS_LOG_WARN ("ProcessOption TDDSS received null pointer");
    return false;
  }

  Ptr<const TdTcpOptionTdDSS> dss = DynamicCast<const TcpOptionTdTcpDSS>(opt);
  if (!dss)
  {
    NS_LOG_WARN ("ProcessOption TDDSS some nonsense");
    return false;
  }

  if (!dss->GetData(ssubflow, sseq)) 
  {
    ssubflow = -1;
  }
  if (!dss->GetAck(dsubflow, sack))
  {
    dsubflow = -1;
  }
  if (ssubflow == -1 && dsubflow == -1) 
  {
    return false;
  }

  return true;
}

void
TdTcpSocketBase::ProcessEstablished (Ptr<Packet> packet, const TcpHeader& tcpHeader)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Extract the flags. PSH, URG, CWR and ECE are disregarded.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG | TcpHeader::CWR | TcpHeader::ECE);
  int16_t ssid = -1, scid = -1, asid = -1, acid = -1;
  uint32_t sseq, sack;


  if (tcpflags == TcpHeader::ACK)
  {
    if (tcpHeader.GetAckNumber () < m_txBuffer->HeadSequence ())
    {
      // Case 1:  If the ACK is a duplicate (SEG.ACK < SND.UNA), it can be ignored.
      // Pag. 72 RFC 793
      NS_LOG_WARN ("Ignored ack of " << tcpHeader.GetAckNumber () <<
                   " SND.UNA = " << m_txBuffer->HeadSequence ());

      // TODO: RFC 5961 5.2 [Blind Data Injection Attack].[Mitigation]
    }
    else if (tcpHeader.GetAckNumber () > m_tcb->m_highTxMark)
    {
      // If the ACK acks something not yet sent (SEG.ACK > HighTxMark) then
      // send an ACK, drop the segment, and return.
      // Pag. 72 RFC 793
      NS_LOG_WARN ("Ignored ack of " << tcpHeader.GetAckNumber () <<
                   " HighTxMark = " << m_tcb->m_highTxMark);
    }
    else
    {
      // SND.UNA < SEG.ACK =< HighTxMark
      // Pag. 72 RFC 793
      ReceivedAck (packet, tcpHeader);
    }
  }
  else if (tcpflags == TcpHeader::SYN)
  { // Received SYN, old NS-3 behaviour is to set state to SYN_RCVD and
    // respond with a SYN+ACK. But it is not a legal state transition as of
    // RFC793. Thus this is ignored.
    return;
  }
  else if (tcpflags == (TcpHeader::SYN | TcpHeader::ACK))
  { // No action for received SYN+ACK, it is probably a duplicated packet
    return;
  } 
  else if (tcpflags == TcpHeader::FIN || tcpflags == (TcpHeader::FIN | TcpHeader::ACK))
  { // Received FIN or FIN+ACK, bring down this socket nicely
    PeerClose (packet, tcpHeader);
    return;
  }
  else if (tcpflags == 0)
  { // No flags means there is only data

  }
  else
  { // Received RST or the TCP flags is invalid, in either case, terminate this socket
    if (tcpflags != TcpHeader::RST)
    { // this must be an invalid flag, send reset
      NS_LOG_LOGIC ("Illegal flag " << TcpHeader::FlagsToString (tcpflags) << " received. Reset packet is sent.");
      SendRST ();
    }
    CloseAndNotify ();
    return;
  }

  if (ProcessOptionTdDSS(tcpHeader.GetOption(TcpOption::TDTCP),
                          ssid, scid, sseq, asid, acid, dseq)) 
  {
    if (ssid >= 0) 
    {
      DeliverDataToSubflow((uint8_t)ssid, (uint8_t)scid, sseq, packet, tcpHeader);
    }
    if (asid >= 0) 
    {
      DeliverAckToSubflow((uint8_t)asid, (uint8_t)acid, sack, packet, tcpHeader);
    }
  }
  //All connection close routine are unimplemented for now...
  // else if (false)ProcessOptionTdFIN(tcpHeader.GetOption(TcpOption::TDTCP))
  // {

  // }
  else 
  {
    NS_LOG_WARN ("Received Packet in TDTCP without TDTCP flag...")
    return;
  }

}

// Receive
void 
TdTcpSocketBase::DeliverDataToSubflow(uint8_t sid,
                         uint8_t cid,
                         uint32_t sseq,
                         Ptr<Packet> packet, 
                         const TcpHeader& tcpHeader)
{
  NS_LOG_FUNCTION (this << subflowid << sseq << tcpHeader);
  NS_LOG_DEBUG ("Data segment, seq=" << tcpHeader.GetSequenceNumber () <<
                " pkt size=" << p->GetSize () );

  Ptr<TdTcpRxSubflow> rxsubflow = m_rxsubflows[subflowid];

  uint32_t packetSize = packet->GetSize();
  TdTcpMapping m = GetMapping(tcpHeader.GetSequenceNumber().GetValue(), sseq, packetSize);
  rxsubflow->m_RxMappings.AddMapping(m);

  rxsubflow->ReceivedData(packet, tcpHeader, SequenceNumber32(sseq), scid);

} 

void 
TdTcpSocketBase::DeliverAckToSubflow(uint8_t asid,
                        uint8_t acid,
                        uint32_t sack,
                        Ptr<Packet> packet, 
                        const TcpHeader& tcpHeader)
{
  NS_LOG_FUNCTION (this << asid << acid << sack << tcpHeader);
  NS_LOG_DEBUG ("ACK segment, seq=" << tcpHeader.GetSequenceNumber () <<
                " pkt size=" << p->GetSize () );

  Ptr<TdTcpTxSubflow> txsubflow = m_txsubflows[subflowid];
  txsubflow->ReceivedAck(acid, packet, tcpHeader, SequenceNumber32(sack));
}

void 
TdTcpSocketBase::ReceivedAck (Ptr<Packet> packet, const TcpHeader& tcpHeader, SequenceNumber32 dack)
{
  NS_LOG_FUNCTION("Received DACK " << dack << "(Enable dupacks:" << count_dupacks << " )");

  if (dack < m_txBuffer->HeadSequence ())
  { // Case 1: Old ACK, ignored.
    NS_LOG_LOGIC ("Old ack Ignored " << dack  );
  }
  else if (dack  == m_txBuffer->HeadSequence ())
  { // Case 2: Potentially a duplicated ACK
    if (dack < m_tcb->m_nextTxSequence)
    {
    /* dupackcount shall only be increased if there is only a DSS option ! */
      m_dupAck++;
    }
    // otherwise, the ACK is precisely equal to the nextTxSequence
    NS_ASSERT(dack <= m_tcb->m_nextTxSequence);
    if (m_dupAck > m_connDupAckTh) 
    {
      Ptr<TdTcpTxSubflow> tx;
      if ((tx = m_seqToSubflowMap[dack]))
      {
        tx->DoRetransmit();
      }
      else 
      {
        NS_FATAL_ERROR ("Cannot find subflow who sent a packet in data dupack event");
      }
    }

  }
  else if (dack > m_txBuffer->HeadSequence ())
  { // Case 3: New ACK, reset m_dupAckCount and update m_txBuffer
    NS_LOG_LOGIC ("New DataAck [" << dack  << "]");
    m_txBuffer->DiscardUpTo( dack );
    m_seqToSubflowMap.erase(m_seqToSubflowMap.begin(), dack);
    NewAck(dack, false);
    m_dupAckCount = 0;
  }
}

void 
TdTcpSocketBase::OnSubflowReceive (Ptr<Packet> packet, 
                               const TcpHeader& tcpHeader, 
                               Ptr<TdTcpRxSubflow> sf,
                               uint32_t sack)
{
  NS_UNUSED(packet);
  NS_UNUSED(tcpHeader);
  NS_UNUSED(sack);

  NS_LOG_FUNCTION(this << "Received data from subflow=" << sf);
  NS_LOG_INFO("=> Dumping meta RxBuffer before extraction");
  DumpRxBuffers(sf);
  SequenceNumber32 expectedDSN = m_rxBuffer->NextRxSequence();

  /* Extract one by one mappings from subflow */
  while(true)
  {
    Ptr<Packet> p;
    SequenceNumber32 dsn;
    uint32_t canRead = m_rxBuffer->MaxBufferSize() - m_rxBuffer->Size();

    if(canRead <= 0)
    {
      NS_LOG_LOGIC("No free space in meta Rx Buffer");
      break;
    }
    /* Todo tell if we stop to extract only between mapping boundaries or if Extract */
    p = sf->ExtractAtMostOneMapping(canRead, true, dsn);
    if (p->GetSize() == 0)
    {
      NS_LOG_DEBUG("packet extracted empty.");
      break;
    }
    // THIS MUST WORK. else we removed the data from subflow buffer so it would be lost
    // Pb here, htis will be extracted but will not be saved into the main buffer
    // Notify app to receive if necessary
    if(!m_rxBuffer->Add(p, =dsn))
    {
      NS_FATAL_ERROR("Data might have been lost");
    }
  }
  NS_LOG_INFO("=> Dumping RxBuffers after extraction");
  DumpRxBuffers(sf);
  if (expectedDSN < m_rxBuffer->NextRxSequence())
  {
    NS_LOG_LOGIC("The Rxbuffer advanced");

    // NextRxSeq advanced, we have something to send to the app
    if (!m_shutdownRecv)
    {
      //<< m_receivedData
      NS_LOG_LOGIC("Notify data Rcvd" );
      NotifyDataRecv();
    }
    // Handle exceptions
    if (m_closeNotified)
    {
      NS_LOG_WARN ("Why TCP " << this << " got data after close notification?");
    }
  }
}

// Send 
uint32_t 
TdTcpSocketBase::SendPendingData (bool withAck)
{
  NS_LOG_FUNCTION (this << withAck);
  if (m_txBuffer->Size () == 0)
  {
    return false;                           // Nothing to send
  }
  if (m_endPoint == nullptr && m_endPoint6 == nullptr)
  {
    NS_LOG_INFO ("TcpSocketBase::SendPendingData: No endpoint; m_shutdownSend=" << m_shutdownSend);
    return false; // Is this the right way to handle this condition?
  }
  Ptr<TdTcpTxSubflow> subflow = m_txsubflows[m_currTxSubflow];

  uint32_t nPacketsSent = 0;
  uint32_t availableWindow = subflow->AvailableWindow ();

  // RFC 6675, Section (C)
  // If cwnd - pipe >= 1 SMSS, the sender SHOULD transmit one or more
  // segments as follows:
  // (NOTE: We check > 0, and do the checks for segmentSize in the following
  // else branch to control silly window syndrome and Nagle)
  while (availableWindow > 0)
  {
    if (m_tcb->m_pacing)
    {
      NS_LOG_INFO ("Pacing is enabled");
      if (subflow->m_pacingTimer.IsRunning ())
      {
        NS_LOG_INFO ("Skipping Packet due to pacing" << m_pacingTimer.GetDelayLeft ());
        break;
      }
      NS_LOG_INFO ("Timer is not running");
    }

    if (subflow->m_tcb->m_congState == TcpSocketState::CA_OPEN
        && m_state == TcpSocket::FIN_WAIT_1)
    {
      NS_LOG_INFO ("FIN_WAIT and OPEN state; no data to transmit");
      break;
    }
    // (C.1) The scoreboard MUST be queried via NextSeg () for the
    //       sequence number range of the next segment to transmit (if
    //       any), and the given segment sent.  If NextSeg () returns
    //       failure (no data to send), return without sending anything
    //       (i.e., terminate steps C.1 -- C.5).
    SequenceNumber32 next;
    // bool enableRule3 = m_sackEnabled && subflow->m_tcb->m_congState == TcpSocketState::CA_RECOVERY;
    if (!m_txBuffer->NextSeg (&next, false))
    {
      NS_LOG_INFO ("no valid seq to transmit, or no data available");
      break;
    }
    else
    {
      // It's time to transmit, but before do silly window and Nagle's check
      uint32_t availableData = m_txBuffer->SizeFromSequence (next);

      // If there's less app data than the full window, ask the app for more
      // data before trying to send
      if (availableData < availableWindow)
      {
        NotifySend (GetTxAvailable ());
      }

      // Stop sending if we need to wait for a larger Tx window (prevent silly window syndrome)
      // but continue if we don't have data
      if (availableWindow < subflow->m_tcb->m_segmentSize && availableData > availableWindow)
      {
        NS_LOG_LOGIC ("Preventing Silly Window Syndrome. Wait to send.");
        break; // No more
      }
      // Nagle's algorithm (RFC896): Hold off sending if there is unacked data
      // in the buffer and the amount of data to send is less than one segment
       
      if (!m_noDelay && UnAckDataCount () > 0 && availableData < subflow->m_tcb->m_segmentSize)
      {
        NS_LOG_DEBUG ("Invoking Nagle's algorithm for seq " << next <<
                      ", SFS: " << m_txBuffer->SizeFromSequence (next) <<
                      ". Wait to send.");
        break;
      }
     
      uint32_t s = std::min (availableWindow, subflow->m_tcb->m_segmentSize);

      // (C.2) If any of the data octets sent in (C.1) are below HighData,
      //       HighRxt MUST be set to the highest sequence number of the
      //       retransmitted segment unless NextSeg () rule (4) was
      //       invoked for this retransmission.
      // (C.3) If any of the data octets sent in (C.1) are above HighData,
      //       HighData must be updated to reflect the transmission of
      //       previously unsent data.
      //
      // These steps are done in m_txBuffer with the tags.
      if (subflow->m_tcb->m_nextTxSequence != next)
      {
        subflow->m_tcb->m_nextTxSequence = next;
      }
      if (subflow->m_tcb->m_bytesInFlight.Get () == 0)
      {
        subflow->m_congestionControl->CwndEvent (subflow->m_tcb, TcpSocketState::CA_EVENT_TX_START);
      }

      SequenceNumber32 dsnHead = m_tcb->m_nextTxSequence;
      
      // For now we limit the mapping to a per packet basis
      bool ok = subflow->AddLooseMapping(dsnHead, s);
      NS_ASSERT(ok);

      // see next #if 0 to see how it should be
      SequenceNumber32 dsnTail = dsnHead + length;
      Ptr<Packet> p = m_txBuffer->CopyFromSequence(length, dsnHead);
      NS_ASSERT(p->GetSize() == length);
      // int ret = subflow->Send(p, 0);

      // By making sure each subflow's txbuffer is at most as big as meta's tx buffer
      // we can guarantee that this will not overflow.
      ok = subflow->m_txBuffer->Add (p);
      NS_ASSERT(ok);

      m_seqToSubflowMap[sdnHead] = subflow;
      
      uint32_t sz = subflow->SendDataPacket (subflow->m_tcb->m_nextTxSequence, s, withAck);

      // For TDTCP we need to make the strong guarantee that the subflow cannot
      // have any pending data, otherwise on a subflow switch these data will 
      // be blocked...
      NS_ASSERT(sz == s);

      subflow->m_tcb->m_nextTxSequence += sz;

      /* Ideally we should be able to send data out of order so that it arrives in order at the
       * receiver but to do that we need SACK support (IMO). Once SACK is implemented it should
       * be reasonably easy to add
       */
      NS_ASSERT(dsnHead == m_tcb->m_nextTxSequence);
      SequenceNumber32 nextTxSeq = m_tcb->m_nextTxSequence;
      if (dsnHead <=  nextTxSeq && (dsnTail) >= nextTxSeq )
      {
        m_tcb->m_nextTxSequence = dsnTail;
      }
      m_tcb->m_highTxMark = std::max( m_tcb->m_highTxMark.Get(), dsnTail);

      NS_LOG_LOGIC("m_nextTxSequence=" << m_tcb->m_nextTxSequence << " m_highTxMark=" << m_tcb->m_highTxMark);

      NS_LOG_LOGIC (" rxwin " << m_rWnd <<
                    " segsize " << subflow->m_tcb->m_segmentSize <<
                    " highestRxAck " << m_txBuffer->HeadSequence () <<
                    " pd->Size " << m_txBuffer->Size () <<
                    " pd->SFS " << m_txBuffer->SizeFromSequence (subflow->m_tcb->m_nextTxSequence));

      NS_LOG_DEBUG ("cWnd: " << subflow->m_tcb->m_cWnd <<
                    " total unAck: " << UnAckDataCount () <<
                    " sent seq " << subflow->m_tcb->m_nextTxSequence <<
                    " size " << sz);
      ++nPacketsSent;

      if (subflow->m_tcb->m_pacing)
      {
        NS_LOG_INFO ("Pacing is enabled");
        if (m_pacingTimer.IsExpired ())
        {
          NS_LOG_DEBUG ("Current Pacing Rate " << subflow->m_tcb->m_currentPacingRate);
          NS_LOG_DEBUG ("Timer is in expired state, activate it " << subflow->m_tcb->m_currentPacingRate.CalculateBytesTxTime (sz));
          m_pacingTimer.Schedule (subflow->m_tcb->m_currentPacingRate.CalculateBytesTxTime (sz));
          break;
        }
      }
    }


    // (C.4) The estimate of the amount of data outstanding in the
    //       network must be updated by incrementing pipe by the number
    //       of octets transmitted in (C.1).
    //
    // Done in BytesInFlight, inside AvailableWindow.
    availableWindow = subflow->AvailableWindow ();

    // (C.5) If cwnd - pipe >= 1 SMSS, return to (C.1)
    // loop again!
  }

  if (nPacketsSent > 0)
  {
    NS_LOG_DEBUG ("SendPendingData sent " << nPacketsSent << " segments");
  }
  else
  {
    NS_LOG_DEBUG ("SendPendingData no segments sent");
  }
  return nPacketsSent;
}
 

void 
TdTcpSocketBase::SendAckPacket (uint8_t subflowid, uint8_t scid, uint32_t sack)
{
  NS_LOG_FUNCTION(this);

  if (m_endPoint == nullptr && m_endPoint6 == nullptr)
  {
    NS_LOG_WARN ("Failed to send empty packet due to null endpoint");
    return;
  }

  Ptr<Packet> p = Create<Packet> ();
  TcpHeader header;
  SequenceNumber32 s = m_tcb->m_nextTxSequence;

  flags |= (TcpHeader::ACK);

  AddSocketTags (p);

  header.SetFlags (flags);
  header.SetSequenceNumber (s);
  header.SetAckNumber (m_rxBuffer->NextRxSequence ());
  if (m_endPoint != nullptr)
  {
    header.SetSourcePort (m_endPoint->GetLocalPort ());
    header.SetDestinationPort (m_endPoint->GetPeerPort ());
  }
  else
  {
    header.SetSourcePort (m_endPoint6->GetLocalPort ());
    header.SetDestinationPort (m_endPoint6->GetPeerPort ());
  }

  AddOptionTDDSS(header, false, 0, 0, 0, true, subflowid, scid, sack);

  windowSize = AdvertisedWindowSize (false);
  m_txTrace (p, header, this);

  if (m_endPoint != nullptr)
  {
    m_tcp->SendPacket (p, header, m_endPoint->GetLocalAddress (),
                       m_endPoint->GetPeerAddress (), m_boundnetdevice);
  }
  else
  {
    m_tcp->SendPacket (p, header, m_endPoint6->GetLocalAddress (),
                       m_endPoint6->GetPeerAddress (), m_boundnetdevice);
  }
}


void
TdTcpSocketBase::ReTxTimeout ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT)
  {
    return;
  }

  if (m_state == SYN_SENT)
  {
    if (m_synCount > 0)
    {
      SendSYN (false);
    }
    else
    {
      NotifyConnectionFailed ();
    }
    return;
  }

  // Retransmit non-data packet: Only if in FIN_WAIT_1 or CLOSING state
  if (m_txBuffer->Size () == 0)
  {
    if (m_state == FIN_WAIT_1 || m_state == CLOSING)
    { // Must have lost FIN, re-send
      SendFIN ();
    }
    return;
  }

  NS_LOG_DEBUG ("Checking if Connection is Established");
  // If all data are received (non-closing socket and nothing to send), just return
  if (m_state <= ESTABLISHED && m_txBuffer->HeadSequence () >= m_tcb->m_highTxMark && m_txBuffer->Size () == 0)
  {
    NS_LOG_DEBUG ("Already Sent full data" << m_txBuffer->HeadSequence () << " " << m_tcb->m_highTxMark);
    return;
  }

  if (m_dataRetrCount == 0)
  {
    NS_LOG_INFO ("No more data retries available. Dropping connection");
    NotifyErrorClose ();
    DeallocateEndPoint ();
    return;
  }
  else
  {
    --m_dataRetrCount;
  }

  Ptr<TdTcpTxSubflow> subflow = m_txSubflows[m_currTxSubflow];

  uint32_t inFlightBeforeRto = subflow->BytesInFlight ();
  // bool resetSack = !m_sackEnabled; // Reset SACK information if SACK is not enabled.
  //                                  // The information in the TcpTxBuffer is guessed, in this case.

  // Reset dupAckCount
  subflow->m_dupAckCount = 0;
  // if (!m_sackEnabled)
  // {
  subflow->m_txBuffer->ResetRenoSack ();
  // }

  subflow->m_txBuffer->SetSentListLost (true);

  // From RFC 6675, Section 5.1
  // If an RTO occurs during loss recovery as specified in this document,
  // RecoveryPoint MUST be set to HighData.  Further, the new value of
  // RecoveryPoint MUST be preserved and the loss recovery algorithm
  // outlined in this document MUST be terminated.
  subflow->m_recover = subflow->m_tcb->m_highTxMark;

  // RFC 6298, clause 2.5, double the timer
  Time doubledRto = subflow->m_rto + subflow->m_rto;
  subflow->m_rto = Min (doubledRto, Time::FromDouble (60,  Time::S));

  // Empty RTT history
  subflow->m_history.clear ();

  // Please don't reset highTxMark, it is used for retransmission detection

  // When a TCP sender detects segment loss using the retransmission timer
  // and the given segment has not yet been resent by way of the
  // retransmission timer, decrease ssThresh
  if (subflow->m_tcb->m_congState != TcpSocketState::CA_LOSS || !subflow->m_txBuffer->IsHeadRetransmitted ())
  {
    subflow->m_tcb->m_ssThresh = subflow->m_congestionControl->GetSsThresh (subflow->m_tcb, inFlightBeforeRto);
  }

  // Cwnd set to 1 MSS
  subflow->m_tcb->m_cWnd = subflow->m_tcb->m_segmentSize;
  subflow->m_tcb->m_cWndInfl = subflow->m_tcb->m_cWnd;
  subflow->m_congestionControl->CwndEvent (subflow->m_tcb, TcpSocketState::CA_EVENT_LOSS);
  subflow->m_congestionControl->CongestionStateSet (subflow->m_tcb, TcpSocketState::CA_LOSS);
  subflow->m_tcb->m_congState = TcpSocketState::CA_LOSS;

  subflow->m_pacingTimer.Cancel ();

  NS_LOG_DEBUG ("RTO. Reset cwnd to " <<  subflow->m_tcb->m_cWnd << ", ssthresh to " <<
                subflow->m_tcb->m_ssThresh << ", restart from seqnum " <<
                subflow->m_txBuffer->HeadSequence () << " doubled rto to " <<
                subflow->m_rto.Get ().GetSeconds () << " s");

  NS_ASSERT_MSG (subflow->BytesInFlight () == 0, "There are some bytes in flight after an RTO: " <<
                 subflow->BytesInFlight ());

  // SendPendingData (m_connected);
  // IDK... this doesn't make sense but what does?
  subflow->DoRetransmit();

  NS_ASSERT_MSG (BytesInFlight () <= subflow->m_tcb->m_segmentSize,
                 "In flight (" << BytesInFlight () <<
                 ") there is more than one segment (" << subflow->m_tcb->m_segmentSize << ")");
}

void
TdTcpSocketBase::ChangeActivateSubflow(uint8_t newsid) 
{
  NS_LOG_FUNCTION (this << newsid);
  NS_ASSERT (newsid < m_txSubflows.size() > newsid);
  m_currentTxSubflow = newsid;
  // Dupack can be made adaptive here. No for now... 
  // Will be enough debug work. 
}

}
