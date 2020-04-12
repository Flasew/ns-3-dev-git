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

#include "tdtcp-tx-subflow.h"
#include "tdtcp-socket-base.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/tdtcp-socket-base.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-end-point.h"
#include "ipv6-end-point.h"
#include "ns3/node.h"
#include "ns3/ptr.h"
#include "ns3/tcp-option-mptcp.h"
#include "ns3/ipv4-address.h"
#include "ns3/trace-helper.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-recovery-ops.h"
#include "ns3/tcp-socket.h"
#include "ns3/tcp-option-ts.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TdTcpTxSubflow");

TypeId 
TdTcpTxSubflow::GetdTypeId (void) 
{
  static TypeId tid = TypeId ("ns3::TdTcpTxSubflow")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
    .AddConstructor<TdTcpTxSubflow> ()
    // .AddTraceSource ("CongestionWindow",
    //              "The TCP connection's congestion window",
    //              MakeTraceSourceAccessor (&TdTcpTxSubflow::m_cWndTrace),
    //              "ns3::TracedValueCallback::Uint32")
  ;
  return tid;
}

TypeId 
TdTcpTxSubflow::GetInstanceTypeId ()
{
  return TdTcpTxSubflow::GetTypeId();
}

TdTcpTxSubflow::TdTcpTxSubflow (uint8_t id, Ptr<TdTcpSocketBase> tdtcp) 
{
  m_meta = tdtcp;
  m_subflowid = id;
  m_txBuffer = CreateObject<TcpTxBuffer> ();
  m_txBuffer->SetMaxBufferSize(m_meta->m_txBuffer->MaxBufferSize());
  m_rtt      = m_meta->m_rtt->Copy ();
  m_tcb      = CopyObject<TcpSocketState> (m_meta->m_tcb);
  m_tcb->m_nextTxSequence = SequenceNumber32(0);
  m_rto      = m_meta->m_rto.Get();
  m_minRto   = m_meta->m_minRto;
  m_clockGranularity = m_meta->m_clockGranularity;

  if (m_meta->m_congestionControl)
  {
    m_congestionControl = m_meta->m_congestionControl->Fork ();
  }
  else 
  {
    m_congestionControl = CreateObject<TcpNewReno>();
  }

  if (m_meta->m_recoveryOps)
  {
    m_recoveryOps = m_meta->m_recoveryOps->Fork ();
  }
  else 
  {
    m_recoveryOps = CreateObject<TcpClassicRecovery>();
  }

  bool ok = m_tcb->TraceConnectWithoutContext ("CongestionWindow",
                                        MakeCallback (&TdTcpTxSubflow::UpdateCwnd, this));
  NS_ASSERT (ok == true);
  ok = m_tcb->TraceConnectWithoutContext ("CongState",
                                        MakeCallback (&TdTcpTxSubflow::UpdateCongState, this));
  NS_ASSERT (ok == true);
  
}

TdTcpTxSubflow::~TdTcpTxSubflow() 
{

}

void 
TdTcpTxSubflow::ReceivedAck(uint8_t acid, Ptr<Packet> p, const TcpHeader& tcpHeader, SequenceNumber32 sack) 
{
  NS_LOG_FUNCTION (this << acid << tcpHeader << sack);

  NS_ASSERT (m_tcb->m_segmentSize > 0);

  SequenceNumber32 ackNumber = sack;
  SequenceNumber32 oldHeadSequence = m_txBuffer->HeadSequence ();

  // if cid is not the same as sid, return the size of the packet to CWND
  // uint32_t nbyteToDiscard = sack - oldHeadSequence;
  // if (acid != m_subflowid) 
  // {
  //   Ptr<TdTcpTxSubflow> carrier = m_meta->m_txsubflows[acid];
  // }

  if (tcpHeader.GetAckNumber() > m_meta->m_highRxAckMark) {
    m_txBuffer->DiscardUpTo (ackNumber);
    m_TxMappings.DiscardUpTo (ackNumber);
  }

  if (SequenceNumber32(sack) > m_highRxAckMark)
  {
    m_highRxAckMark = SequenceNumber32(sack);
  }

  // if (m_meta->m_currTxSubflow == m_subflowid)
  // {
    EstimateRtt(tcpHeader, ackNumber);
  // }
  // else 
  // {
  //   m_lastAckDTime = Simulator::Now();
  // }

  // RFC 6675 Section 5: 2nd, 3rd paragraph and point (A), (B) implementation
  // are inside the function ProcessAck
  ProcessAck (ackNumber, oldHeadSequence);

  UpdateAdaptivePacingRate(false);
  // RFC 6675, Section 5, point (C), try to send more data. NB: (C) is implemented
  // inside SendPendingData
  if (m_meta->m_currTxSubflow == m_subflowid && !m_guarded)
  {
    m_meta->SendPendingData (m_meta->m_connected);
  }
}

void
TdTcpTxSubflow::ProcessAck (const SequenceNumber32 &ackNumber, 
                           const SequenceNumber32 &oldHeadSequence)
{
  NS_LOG_FUNCTION (this << ackNumber << oldHeadSequence);
  // RFC 6675, Section 5, 2nd paragraph:
  // If the incoming ACK is a cumulative acknowledgment, the TCP MUST
  // reset DupAcks to zero.
  bool exitedFastRecovery = false;
  uint32_t oldDupAckCount = m_dupAckCount; // remember the old value
  m_tcb->m_lastAckedSeq = ackNumber; // Update lastAckedSeq

  /* In RFC 5681 the definition of duplicate acknowledgment was strict:
   *
   * (a) the receiver of the ACK has outstanding data,
   * (b) the incoming acknowledgment carries no data,
   * (c) the SYN and FIN bits are both off,
   * (d) the acknowledgment number is equal to the greatest acknowledgment
   *     received on the given connection (TCP.UNA from [RFC793]),
   * (e) the advertised window in the incoming acknowledgment equals the
   *     advertised window in the last incoming acknowledgment.
   *
   * With RFC 6675, this definition has been reduced:
   *
   * (a) the ACK is carrying a SACK block that identifies previously
   *     unacknowledged and un-SACKed octets between HighACK (TCP.UNA) and
   *     HighData (m_highTxMark)
   */

  // No sack for now
  bool isDupack = (ackNumber == oldHeadSequence &&
      ackNumber < m_tcb->m_highTxMark);

  NS_LOG_DEBUG ("ACK of " << ackNumber <<
                " SND.UNA=" << oldHeadSequence <<
                " SND.NXT=" << m_tcb->m_nextTxSequence <<
                " in state: " << TcpSocketState::TcpCongStateName[m_tcb->m_congState] <<
                " with m_recover: " << m_recover);

  // RFC 6675, Section 5, 3rd paragraph:
  // If the incoming ACK is a duplicate acknowledgment per the definition
  // in Section 2 (regardless of its status as a cumulative
  // acknowledgment), and the TCP is not currently in loss recovery
  if (isDupack)
  {
    // loss recovery check is done inside this function thanks to
    // the congestion state machine
    DupAck ();
  }
  // else
  // {

  // }

  if (ackNumber == oldHeadSequence
      && ackNumber == m_tcb->m_highTxMark)
  {
    // Dupack, but the ACK is precisely equal to the nextTxSequence
    return;
  }
  else if (ackNumber == oldHeadSequence
           && ackNumber > m_tcb->m_highTxMark)
  {
    // ACK of the FIN bit ... nextTxSequence is not updated since we
    // don't have anything to transmit
    NS_LOG_DEBUG ("Update nextTxSequence manually to " << ackNumber);
    m_tcb->m_nextTxSequence = ackNumber;
  }
  else if (ackNumber == oldHeadSequence)
  {
    // DupAck. Artificially call PktsAcked: after all, one segment has been ACKed.
    m_congestionControl->PktsAcked (m_tcb, 1, m_tcb->m_lastRtt);
  }
  else if (ackNumber > oldHeadSequence)
  {
    // Please remember that, with SACK, we can enter here even if we
    // received a dupack.
    uint32_t bytesAcked = ackNumber - oldHeadSequence;
    uint32_t segsAcked  = bytesAcked / m_tcb->m_segmentSize;
    m_bytesAckedNotProcessed += bytesAcked % m_tcb->m_segmentSize;

    if (m_bytesAckedNotProcessed >= m_tcb->m_segmentSize)
    {
      segsAcked += 1;
      m_bytesAckedNotProcessed -= m_tcb->m_segmentSize;
    }

    // Dupack count is reset to eventually fast-retransmit after 3 dupacks.
    // Any SACK-ed segment will be cleaned up by DiscardUpTo.
    // In the case that we advanced SND.UNA, but the ack contains SACK blocks,
    // we do not reset. At the third one we will retransmit.
    // If we are already in recovery, this check is useless since dupAcks
    // are not considered in this phase. When from Recovery we go back
    // to open, then dupAckCount is reset anyway.
    if (!isDupack)
    {
      m_dupAckCount = 0;
    }

    // RFC 6675, Section 5, part (B)
    // (B) Upon receipt of an ACK that does not cover RecoveryPoint, the
    // following actions MUST be taken:
    //
    // (B.1) Use Update () to record the new SACK information conveyed
    //       by the incoming ACK.
    // (B.2) Use SetPipe () to re-calculate the number of octets still
    //       in the network.
    //
    // (B.1) is done at the beginning, while (B.2) is delayed to part (C) while
    // trying to transmit with SendPendingData. We are not allowed to exit
    // the CA_RECOVERY phase. Just process this partial ack (RFC 5681)
    if (ackNumber < m_recover && m_tcb->m_congState == TcpSocketState::CA_RECOVERY)
    {
      // if (!m_sackEnabled)
      //   {
          // Manually set the head as lost, it will be retransmitted.
      NS_LOG_INFO ("Partial ACK. Manually setting head as lost");
      m_txBuffer->MarkHeadAsLost ();
      //   }
      // else
      //   {
      //     // We received a partial ACK, if we retransmitted this segment
      //     // probably is better to retransmit it
      //     m_txBuffer->DeleteRetransmittedFlagFromHead ();
      //   }
      DoRetransmit (); // Assume the next seq is lost. Retransmit lost packet
      m_tcb->m_cWndInfl = SafeSubtraction (m_tcb->m_cWndInfl, bytesAcked);
      if (segsAcked >= 1)
      {
        m_recoveryOps->DoRecovery (m_tcb, bytesAcked, m_txBuffer->GetSacked ());
      }

      // This partial ACK acknowledge the fact that one segment has been
      // previously lost and now successfully received. All others have
      // been processed when they come under the form of dupACKs
      m_congestionControl->PktsAcked (m_tcb, 1, m_tcb->m_lastRtt);
      NewAck (ackNumber, m_isFirstPartialAck);

      if (m_isFirstPartialAck)
      {
        NS_LOG_DEBUG ("Partial ACK of " << ackNumber <<
                      " and this is the first (RTO will be reset);"
                      " cwnd set to " << m_tcb->m_cWnd <<
                      " recover seq: " << m_recover <<
                      " dupAck count: " << m_dupAckCount);
        m_isFirstPartialAck = false;
      }
      else
      {
        NS_LOG_DEBUG ("Partial ACK of " << ackNumber <<
                      " and this is NOT the first (RTO will not be reset)"
                      " cwnd set to " << m_tcb->m_cWnd <<
                      " recover seq: " << m_recover <<
                      " dupAck count: " << m_dupAckCount);
      }
    }
    // From RFC 6675 section 5.1
    // In addition, a new recovery phase (as described in Section 5) MUST NOT
    // be initiated until HighACK is greater than or equal to the new value
    // of RecoveryPoint.
    else if (ackNumber < m_recover && m_tcb->m_congState == TcpSocketState::CA_LOSS)
    {
      m_congestionControl->PktsAcked (m_tcb, segsAcked, m_tcb->m_lastRtt);
      m_congestionControl->IncreaseWindow (m_tcb, segsAcked);

      NS_LOG_DEBUG (" Cong Control Called, cWnd=" << m_tcb->m_cWnd <<
                    " ssTh=" << m_tcb->m_ssThresh);
      // if (!m_sackEnabled)
      //   {
      //     NS_ASSERT_MSG (m_txBuffer->GetSacked () == 0,
      //                    "Some segment got dup-acked in CA_LOSS state: " <<
      //                    m_txBuffer->GetSacked ());
      //   }
      NewAck (ackNumber, true);
    }
    else
    {
      if (m_tcb->m_congState == TcpSocketState::CA_OPEN)
      {
        m_congestionControl->PktsAcked (m_tcb, segsAcked, m_tcb->m_lastRtt);
      }
      else if (m_tcb->m_congState == TcpSocketState::CA_DISORDER)
      {
        if (segsAcked >= oldDupAckCount)
        {
          m_congestionControl->PktsAcked (m_tcb, segsAcked - oldDupAckCount, m_tcb->m_lastRtt);
        }

        if (!isDupack)
        {
          // The network reorder packets. Linux changes the counting lost
          // packet algorithm from FACK to NewReno. We simply go back in Open.
          m_congestionControl->CongestionStateSet (m_tcb, TcpSocketState::CA_OPEN);
          m_tcb->m_congState = TcpSocketState::CA_OPEN;

          NS_LOG_DEBUG (segsAcked << " segments acked in CA_DISORDER, ack of " <<
                        ackNumber << " exiting CA_DISORDER -> CA_OPEN");

        }
        else
        {
          NS_LOG_DEBUG (segsAcked << " segments acked in CA_DISORDER, ack of " <<
                        ackNumber << " but still in CA_DISORDER");
        }
      }
      // RFC 6675, Section 5:
      // Once a TCP is in the loss recovery phase, the following procedure
      // MUST be used for each arriving ACK:
      // (A) An incoming cumulative ACK for a sequence number greater than
      // RecoveryPoint signals the end of loss recovery, and the loss
      // recovery phase MUST be terminated.  Any information contained in
      // the scoreboard for sequence numbers greater than the new value of
      // HighACK SHOULD NOT be cleared when leaving the loss recovery
      // phase.
      else if (m_tcb->m_congState == TcpSocketState::CA_RECOVERY)
      {
        m_isFirstPartialAck = true;

        // Recalculate the segs acked, that are from m_recover to ackNumber
        // (which are the ones we have not passed to PktsAcked and that
        // can increase cWnd)
        segsAcked = static_cast<uint32_t>(ackNumber - m_recover) / m_tcb->m_segmentSize;
        m_congestionControl->PktsAcked (m_tcb, segsAcked, m_tcb->m_lastRtt);
        m_congestionControl->CwndEvent (m_tcb, TcpSocketState::CA_EVENT_COMPLETE_CWR);
        m_congestionControl->CongestionStateSet (m_tcb, TcpSocketState::CA_OPEN);
        m_tcb->m_congState = TcpSocketState::CA_OPEN;
        exitedFastRecovery = true;
        m_dupAckCount = 0; // From recovery to open, reset dupack

        NS_LOG_DEBUG (segsAcked << " segments acked in CA_RECOVER, ack of " <<
                      ackNumber << ", exiting CA_RECOVERY -> CA_OPEN");
        UpdateAdaptivePacingRate(true);
        // std::cout << "exiting CA_RECOVERY -> CA_OPEN, AvailableWindow=" << AvailableWindow() << std::endl; 
      }
      else if (m_tcb->m_congState == TcpSocketState::CA_LOSS)
      {
        m_isFirstPartialAck = true;

        // Recalculate the segs acked, that are from m_recover to ackNumber
        // (which are the ones we have not passed to PktsAcked and that
        // can increase cWnd)
        segsAcked = (ackNumber - m_recover) / m_tcb->m_segmentSize;

        m_congestionControl->PktsAcked (m_tcb, segsAcked, m_tcb->m_lastRtt);

        m_congestionControl->CongestionStateSet (m_tcb, TcpSocketState::CA_OPEN);
        m_tcb->m_congState = TcpSocketState::CA_OPEN;
        NS_LOG_DEBUG (segsAcked << " segments acked in CA_LOSS, ack of" <<
                      ackNumber << ", exiting CA_LOSS -> CA_OPEN");
        // UpdateAdaptivePacingRate(true);
      }

      if (exitedFastRecovery)
      {
        NewAck (ackNumber, true);
        m_recoveryOps->ExitRecovery (m_tcb);
        NS_LOG_DEBUG ("Leaving Fast Recovery; BytesInFlight() = " <<
                      BytesInFlight () << "; cWnd = " << m_tcb->m_cWnd);
      }
      else
      {
        m_congestionControl->IncreaseWindow (m_tcb, segsAcked);

        m_tcb->m_cWndInfl = m_tcb->m_cWnd;

        NS_LOG_LOGIC ("Congestion control called: " <<
                      " cWnd: " << m_tcb->m_cWnd <<
                      " ssTh: " << m_tcb->m_ssThresh <<
                      " segsAcked: " << segsAcked);

        NewAck (ackNumber, true);
      }
    }
  }
}

void
TdTcpTxSubflow::EnterRecovery ()
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_tcb->m_congState != TcpSocketState::CA_RECOVERY);

  NS_LOG_DEBUG (TcpSocketState::TcpCongStateName[m_tcb->m_congState] <<
                " -> CA_RECOVERY");

  // if (!m_sackEnabled)
  //   {
      // One segment has left the network, PLUS the head is lost
      m_txBuffer->AddRenoSack ();
      m_txBuffer->MarkHeadAsLost ();
  //   }
  // else
  //   {
  //     if (!m_txBuffer->IsLost (m_txBuffer->HeadSequence ()))
  //       {
  //         // We received 3 dupacks, but the head is not marked as lost
  //         // (received less than 3 SACK block ahead).
  //         // Manually set it as lost.
  //         m_txBuffer->MarkHeadAsLost ();
  //       }
  //   }

  // RFC 6675, point (4):
  // (4) Invoke fast retransmit and enter loss recovery as follows:
  // (4.1) RecoveryPoint = HighData
  m_recover = m_tcb->m_highTxMark;

  m_congestionControl->CongestionStateSet (m_tcb, TcpSocketState::CA_RECOVERY);
  m_tcb->m_congState = TcpSocketState::CA_RECOVERY;

  // (4.2) ssthresh = cwnd = (FlightSize / 2)
  // If SACK is not enabled, still consider the head as 'in flight' for
  // compatibility with old ns-3 versions
  uint32_t bytesInFlight = BytesInFlight () + m_tcb->m_segmentSize;
  m_tcb->m_ssThresh = m_congestionControl->GetSsThresh (m_tcb, bytesInFlight);
  m_recoveryOps->EnterRecovery (m_tcb, m_dupAckCount, UnAckDataCount (), m_txBuffer->GetSacked ());

  NS_LOG_INFO (m_dupAckCount << " dupack. Enter fast recovery mode." <<
               "Reset cwnd to " << m_tcb->m_cWnd << ", ssthresh to " <<
               m_tcb->m_ssThresh << " at fast recovery seqnum " << m_recover <<
               " calculated in flight: " << bytesInFlight);

  // (4.3) Retransmit the first data segment presumed dropped
  DoRetransmit ();
  // (4.4) Run SetPipe ()
  // (4.5) Proceed to step (C)
  // these steps are done after the ProcessAck function (SendPendingData)
}

void
TdTcpTxSubflow::DupAck ()
{
  NS_LOG_FUNCTION (this);
  // NOTE: We do not count the DupAcks received in CA_LOSS, because we
  // don't know if they are generated by a spurious retransmission or because
  // of a real packet loss. With SACK, it is easy to know, but we do not consider
  // dupacks. Without SACK, there are some euristics in the RFC 6582, but
  // for now, we do not implement it, leading to ignoring the dupacks.
  if (m_tcb->m_congState == TcpSocketState::CA_LOSS)
  {
    return;
  }

  // RFC 6675, Section 5, 3rd paragraph:
  // If the incoming ACK is a duplicate acknowledgment per the definition
  // in Section 2 (regardless of its status as a cumulative
  // acknowledgment), and the TCP is not currently in loss recovery
  // the TCP MUST increase DupAcks by one ...
  if (m_tcb->m_congState != TcpSocketState::CA_RECOVERY)
  {
    ++m_dupAckCount;
  }

  if (m_tcb->m_congState == TcpSocketState::CA_OPEN)
  {
    // From Open we go Disorder
    NS_ASSERT_MSG (m_dupAckCount == 1, "From OPEN->DISORDER but with " <<
                   m_dupAckCount << " dup ACKs");

    m_congestionControl->CongestionStateSet (m_tcb, TcpSocketState::CA_DISORDER);
    m_tcb->m_congState = TcpSocketState::CA_DISORDER;

    NS_LOG_DEBUG ("CA_OPEN -> CA_DISORDER");
  }

  if (m_tcb->m_congState == TcpSocketState::CA_RECOVERY)
  {
    // if (!m_sackEnabled)
    // {
      // If we are in recovery and we receive a dupack, one segment
      // has left the network. This is equivalent to a SACK of one block.
      m_txBuffer->AddRenoSack ();
    // }
    m_recoveryOps->DoRecovery (m_tcb, 0, m_txBuffer->GetSacked ());
    NS_LOG_INFO (m_dupAckCount << " Dupack received in fast recovery mode."
                 "Increase cwnd to " << m_tcb->m_cWnd);
  }
  else if (m_tcb->m_congState == TcpSocketState::CA_DISORDER)
  {
    // RFC 6675, Section 5, continuing:
    // ... and take the following steps:
    // (1) If DupAcks >= DupThresh, go to step (4).
    if ((m_dupAckCount == m_retxThresh) && (m_highRxAckMark >= m_recover))
    {
      EnterRecovery ();
      NS_ASSERT (m_tcb->m_congState == TcpSocketState::CA_RECOVERY);
    }
    // (2) If DupAcks < DupThresh but IsLost (HighACK + 1) returns true
    // (indicating at least three segments have arrived above the current
    // cumulative acknowledgment point, which is taken to indicate loss)
    // go to step (4).
    else if (m_txBuffer->IsLost (m_highRxAckMark + m_tcb->m_segmentSize))
    {
      EnterRecovery ();
      NS_ASSERT (m_tcb->m_congState == TcpSocketState::CA_RECOVERY);
    }
    else
    {
      // (3) The TCP MAY transmit previously unsent data segments as per
      // Limited Transmit [RFC5681] ...except that the number of octets
      // which may be sent is governed by pipe and cwnd as follows:
      //
      // (3.1) Set HighRxt to HighACK.
      // Not clear in RFC. We don't do this here, since we still have
      // to retransmit the segment.

      // if (!m_sackEnabled && m_limitedTx)
      // {
        m_txBuffer->AddRenoSack ();

        // In limited transmit, cwnd Infl is not updated.
      // }
    }
  }
}

void
TdTcpTxSubflow::DoRetransmit ()
{
  NS_LOG_FUNCTION (this);
  bool res;
  SequenceNumber32 seq;

  // Find the first segment marked as lost and not retransmitted. With Reno,
  // that should be the head
  res = m_txBuffer->NextSeg (&seq, false);
  if (!res)
  {
    // We have already retransmitted the head. However, we still received
    // three dupacks, or the RTO expired, but no data to transmit.
    // Therefore, re-send again the head.
    seq = m_txBuffer->HeadSequence ();
  }
  //NS_ASSERT (m_sackEnabled || seq == m_txBuffer->HeadSequence ());
  NS_ASSERT (seq == m_txBuffer->HeadSequence ());
  NS_LOG_INFO ("Retransmitting " << seq);

  // Update the trace and retransmit the segment
  m_tcb->m_nextTxSequence = seq;
  uint32_t sz = 
    SendDataPacket (m_tcb->m_nextTxSequence, m_tcb->m_segmentSize, true);

  NS_ASSERT (sz > 0);
}

/* this method just retransmits whatever current nextTxSequence is */
void
TdTcpTxSubflow::RetransmitPacket ()
{
  NS_LOG_FUNCTION (this);

  uint32_t sz = 
    SendDataPacket (m_tcb->m_nextTxSequence, m_tcb->m_segmentSize, true);

  NS_ASSERT (sz > 0);
  mtcb->m_nextTxSequence += sz;
}

void
TdTcpTxSubflow::NewAck (SequenceNumber32 const& ack, bool resetRTO)
{
  NS_LOG_FUNCTION (this << ack);

  // Reset the data retransmission count. We got a new ACK!
  // m_dataRetrCount = m_dataRetries;

  // if (m_state != SYN_RCVD && resetRTO)
  // { // Set RTO unless the ACK is received in SYN_RCVD state
  NS_LOG_LOGIC (this << " Cancelled ReTxTimeout event which was set to expire at " <<
                (Simulator::Now () + Simulator::GetDelayLeft (m_meta->m_retxEvent)).GetSeconds ());
  m_meta->m_retxEvent.Cancel ();
  // On receiving a "New" ack we restart retransmission timer .. RFC 6298
  // RFC 6298, clause 2.4
  m_rto = Max (m_rtt->GetEstimate () + Max (m_clockGranularity, m_rtt->GetVariation () * 4), m_minRto);
  NS_LOG_LOGIC ("SubflowID" << m_subflowid << " last RTT: " << (m_tcb->m_lastRtt.Get()).GetSeconds());

  NS_LOG_LOGIC (this << " Schedule ReTxTimeout at time " <<
                Simulator::Now ().GetSeconds () << " to expire at time " <<
                (Simulator::Now () + m_rto).GetSeconds ());
  m_meta->m_retxEvent = Simulator::Schedule (m_rto, &TdTcpSocketBase::ReTxTimeout, m_meta);
  // }

  // Note the highest ACK and tell app to send more
  NS_LOG_LOGIC ("TCP " << this << " NewAck " << ack <<
                " numberAck " << (ack - m_txBuffer->HeadSequence ())); // Number bytes ack'ed

  // if (m_meta->m_currTxSubflow == m_subflowid)
  // {
  //   m_paced = false;
  // }

  if (m_txBuffer->Available () > 0)
  {
    m_meta->NotifySend (m_txBuffer->Available ());
  }
  if (ack > m_tcb->m_nextTxSequence)
  {
    m_tcb->m_nextTxSequence = ack; // If advanced
  }
  if (m_txBuffer->Size () == 0 && m_meta->m_state != TcpSocket::FIN_WAIT_1 && m_meta->m_state != TcpSocket::CLOSING)
  { // No retransmit timer if no data to retransmit
    NS_LOG_LOGIC (this << " Cancelled ReTxTimeout event which was set to expire at " <<
                  (Simulator::Now () + Simulator::GetDelayLeft (m_meta->m_retxEvent)).GetSeconds ());
    m_meta->m_retxEvent.Cancel ();
  }
}

uint32_t
TdTcpTxSubflow::SendDataPacket (SequenceNumber32 seq,  
                                uint32_t maxSize, 
                                bool withAck)
{
  NS_LOG_FUNCTION (this << seq << maxSize << withAck);

  bool isRetransmission = false;
  if (seq != m_tcb->m_highTxMark)
  {
    isRetransmission = true;
  }

  Ptr<Packet> p = m_txBuffer->CopyFromSequence (maxSize, seq);
  uint32_t sz = p->GetSize (); // Size of packet
  uint8_t flags = withAck ? TcpHeader::ACK : 0;
  uint32_t remainingData = m_txBuffer->SizeFromSequence (seq + SequenceNumber32 (sz));

  if (m_tcb->m_pacing)
  {
    NS_LOG_INFO ("Pacing is enabled");
    if (m_meta->m_pacingTimer.IsExpired ())
    {
      NS_LOG_DEBUG ("Current Pacing Rate " << m_tcb->m_currentPacingRate);
      NS_LOG_DEBUG ("Timer is in expired state, activate it " << m_tcb->m_currentPacingRate.CalculateBytesTxTime (sz));
      m_meta->m_pacingTimer.Schedule (m_tcb->m_currentPacingRate.CalculateBytesTxTime (sz));
    }
    else
    {
      NS_LOG_INFO ("Timer is already in running state");
    }
  }

  m_meta->AddSocketTags (p);
  
  TdTcpMapping mapping;
  bool result = m_TxMappings.GetMappingForSSN(seq, mapping);
  if (!result)
  {
    m_TxMappings.Dump();
    NS_FATAL_ERROR("Could not find mapping associated to ssn");
  }
  SequenceNumber32 dseq;
  result = mapping.TranslateSSNToDSN(seq, dseq);
  if (!result)
  {
    NS_FATAL_ERROR("Could not translate mapping associated to ssn");
  }

  /*
  if (m_meta->m_currTxSubflow != m_subflowid)
  {
    Ptr<TdTcpTxSubflow> carrier = m_meta->m_txsubflows[m_meta->m_currTxSubflow];

    m_meta->m_seqXRetransmit.insert({dseq + SequenceNumber32(sz), std::make_pair(carrier, sz)});

    m_meta->m_pacingTimer.Cancel();
    m_meta->m_pacingTimer.Schedule (carrier->m_tcb->m_currentPacingRate.CalculateBytesTxTime (sz));
    carrier->m_paced = true;
    // carrier->m_nbytesSentLastRound += sz;

    // uint32_t carrierCWND = carrier->m_tcb->m_cWnd;
    // carrier->m_tcb->m_cWnd = std::max(carrierCWND - sz, 2*carrier->m_tcb->m_segmentSize);
    // // m_tcb->m_cWnd = std::max(m_tcb->m_cWnd.Get() - sz, 2*m_tcb->m_segmentSize);
    // carrier->m_transmitForOther += sz;
    // NS_LOG_INFO ("Subflow " << (int)m_meta->m_currTxSubflow << " carried other info, transmit for other=" << carrier->m_transmitForOther);
    isRetransmission = true;
  }
  */

  // else 
  // {
  //   m_nbytesSentLastRound += sz;
  // }
  TcpHeader header;
  header.SetFlags (flags);
  header.SetSequenceNumber (dseq);
  header.SetAckNumber (m_meta->m_rxBuffer->NextRxSequence ());

  if (m_meta->m_endPoint)
  {
    header.SetSourcePort (m_meta->m_endPoint->GetLocalPort ());
    header.SetDestinationPort (m_meta->m_endPoint->GetPeerPort ());
  }
  else
  {
    header.SetSourcePort (m_meta->m_endPoint6->GetLocalPort ());
    header.SetDestinationPort (m_meta->m_endPoint6->GetPeerPort ());
  }
  header.SetWindowSize (m_meta->AdvertisedWindowSize ());

  // AddOptions (header);
  // m_meta->AddOptionTdTcpDSS(header, true, m_subflowid, m_meta->m_currTxSubflow, seq.GetValue(),
  //                               false, 0, 0, 0);
  m_meta->AddOptionTdTcpDSS(header, true, m_subflowid, m_subflowid, seq.GetValue(),
                              false, 0, 0, 0);
  if (m_meta->m_timestampEnabled)
  {
    m_meta->AddOptionTimestamp (header);
  }

  if (m_meta->m_retxEvent.IsExpired ())
  {
    // Schedules retransmit timeout. m_rto should be already doubled.

    NS_LOG_LOGIC (this << " SendDataPacket Schedule ReTxTimeout at time " <<
                  Simulator::Now ().GetSeconds () << " to expire at time " <<
                  (Simulator::Now () + m_rto).GetSeconds () );
    m_meta->m_retxEvent = Simulator::Schedule (m_rto, &TdTcpSocketBase::ReTxTimeout, m_meta);
  }

  m_meta->m_txTrace (p, header, m_meta);

  if (m_meta->m_endPoint)
  {
    m_meta->m_tcp->SendPacket (p, header, m_meta->m_endPoint->GetLocalAddress (),
                         m_meta->m_endPoint->GetPeerAddress (), m_meta->m_boundnetdevice);
    NS_LOG_DEBUG ("Send segment of size " << sz << " with remaining data " <<
                    remainingData << " via TcpL4Protocol to " <<  m_meta->m_endPoint->GetPeerAddress () <<
                    ". Header " << header);
  }
  else
  {
    m_meta->m_tcp->SendPacket (p, header, m_meta->m_endPoint6->GetLocalAddress (),
                       m_meta->m_endPoint6->GetPeerAddress (), m_meta->m_boundnetdevice);
    NS_LOG_DEBUG ("Send segment of size " << sz << " with remaining data " <<
                  remainingData << " via TcpL4Protocol to " <<  m_meta->m_endPoint6->GetPeerAddress () <<
                    ". Header " << header);
  }

  UpdateRttHistory (seq, sz, isRetransmission);

  // Update bytes sent during recovery phase
  if (m_tcb->m_congState == TcpSocketState::CA_RECOVERY)
  {
    m_recoveryOps->UpdateBytesSent (sz);
  }

  // Notify the application of the data being sent unless this is a retransmit
  if (dseq + sz > m_meta->m_tcb->m_highTxMark)
  {
    Simulator::ScheduleNow (&TdTcpSocketBase::NotifyDataSent, m_meta,
                              (dseq + sz - m_meta->m_tcb->m_highTxMark.Get ()));
  }
  // Update highTxMark
  m_tcb->m_highTxMark = std::max (seq + sz, m_tcb->m_highTxMark.Get ());
  return sz;
}

uint32_t
TdTcpTxSubflow::AvailableWindow () const
{
  uint32_t win = Window ();             // Number of bytes allowed to be outstanding
  uint32_t inflight = BytesInFlight (); // Number of outstanding bytes
  return (inflight > win) ? 0 : win - inflight;
}

uint32_t
TdTcpTxSubflow::Window (void) const
{
  return std::min (m_meta->m_rWnd.Get (), m_tcb->m_cWnd.Get ());
}

uint32_t
TdTcpTxSubflow::SafeSubtraction (uint32_t a, uint32_t b)
{
  if (a > b)
    {
      return a-b;
    }

  return 0;
}

uint32_t
TdTcpTxSubflow::BytesInFlight () const
{
  uint32_t bytesInFlight = m_txBuffer->BytesInFlight ();
  for (auto it = m_meta->m_seqXRetransmit.begin(); it != m_meta->m_seqXRetransmit.end(); it++)
  {
    if (it->second.first == this) 
    {
      bytesInFlight += it->second.second;
      // std::cerr << "ACCOUNTING " << it->second.second << " CROSS FLOW DATA " << std::endl;
    }
  }
  // Ugly, but we are not modifying the state; m_bytesInFlight is used
  // only for tracing purpose.
  m_tcb->m_bytesInFlight = bytesInFlight;

  NS_LOG_DEBUG ("Returning calculated bytesInFlight: " << bytesInFlight);
  return bytesInFlight;
}

uint32_t
TdTcpTxSubflow::UnAckDataCount () const
{
  return m_tcb->m_highTxMark - m_txBuffer->HeadSequence ();
}

void
TdTcpTxSubflow::UpdateRttHistory (const SequenceNumber32 &seq, uint32_t sz,
                                 bool isRetransmission)
{
  NS_LOG_FUNCTION (this);

  // update the history of sequence numbers used to calculate the RTT
  if (isRetransmission == false)
  { // This is the next expected one, just log at end
    m_history.push_back (RttHistory (seq, sz, Simulator::Now ()));
  }
  else
  { // This is a retransmit, find in list and mark as re-tx
    for (std::deque<RttHistory>::iterator i = m_history.begin (); i != m_history.end (); ++i)
    {
      if ((seq >= i->seq) && (seq < (i->seq + SequenceNumber32 (i->count))))
      { // Found it
        i->retx = true;
        i->count = ((seq + SequenceNumber32 (sz)) - i->seq); // And update count in hist
        break;
      }
    }
  }
}

bool
TdTcpTxSubflow::AddLooseMapping(SequenceNumber32 dsnHead, uint16_t length)
{
  NS_LOG_LOGIC("Adding mapping with dsn=" << dsnHead << " len=" << length);
  TdTcpMapping mapping;

  // outssn = FirstUnmappedSSN();
  mapping.MapToSSN(FirstUnmappedSSN());
  mapping.SetMappingSize(length);
  mapping.SetHeadDSN(dsnHead);
  bool ok = m_TxMappings.AddMapping(mapping);

  NS_ASSERT_MSG( ok, "Can't add mapping: 2 mappings overlap");
  return ok;
}

SequenceNumber32
TdTcpTxSubflow::FirstUnmappedSSN()
{
  NS_LOG_FUNCTION(this);
  SequenceNumber32 ssn;
  if(!m_TxMappings.FirstUnmappedSSN(ssn))
  {
    ssn = m_txBuffer->TailSequence();
  }
  return ssn;
}

void
TdTcpTxSubflow::EstimateRtt (const TcpHeader& tcpHeader, const SequenceNumber32 & ackNumber)
{
  SequenceNumber32 ackSeq = ackNumber;
  Time m = Time (0.0);
  bool mzero = false;
  // An ack has been received, calculate rtt and log this measurement
  // Note we use a linear search (O(n)) for this since for the common
  // case the ack'ed packet will be at the head of the list
  // if (!m_history.empty ())
  //   {
  //     RttHistory& h = m_history.front ();

  //   }

  if (m_history.front().retx)
  {
    mzero = true;
  } 
  // RttHistory t;
  // Now delete all ack history with seq <= ack
  while (!m_history.empty ())
    {
      RttHistory& h = m_history.front ();
      if (!h.retx && ackSeq >= (h.seq + SequenceNumber32 (h.count)) && !mzero)
        { // Ok to use this sample
          if (m_meta->m_timestampEnabled && tcpHeader.HasOption (TcpOption::TS))
            {
              Ptr<const TcpOptionTS> ts;
              ts = DynamicCast<const TcpOptionTS> (tcpHeader.GetOption (TcpOption::TS));
              m = TcpOptionTS::ElapsedTimeFromTsValue (ts->GetEcho ());
            }
          else
            {
              m = Simulator::Now () - h.time; // Elapsed time
            }
        }
      else if ((h.seq + SequenceNumber32 (h.count)) > ackSeq)
        {
          break;                                                              // Done removing
        }
      else 
      {
        m = Time (0.0);
      }
      m_history.pop_front (); // Remove
    }
  NS_LOG_LOGIC ("Subflow " << (int)m_subflowid 
    << " measured RTT of ack=" << ackNumber.GetValue() << ": " << m.GetSeconds());

  if (!m.IsZero ())
    {
      m_rtt->Measurement (m);                // Log the measurement
      // RFC 6298, clause 2.4
      m_rto = Max (m_rtt->GetEstimate () + Max (m_clockGranularity, m_rtt->GetVariation () * 4), m_minRto);
      m_tcb->m_lastRtt = m_rtt->GetEstimate ();
      m_tcb->m_minRtt = std::min (m_tcb->m_lastRtt.Get (), m_tcb->m_minRtt);
      NS_LOG_INFO (this << m_tcb->m_lastRtt << m_tcb->m_minRtt);
    }
}

void
TdTcpTxSubflow::UpdateCwnd (uint32_t oldValue, uint32_t newValue)
{
  m_cWndTrace (m_meta, m_subflowid, oldValue, newValue);
}

void
TdTcpTxSubflow::UpdateCongState (TcpSocketState::TcpCongState_t oldValue,
                                TcpSocketState::TcpCongState_t newValue)
{
  m_congStateTrace (m_meta, m_subflowid, oldValue, newValue);
}

void 
TdTcpTxSubflow::UpdateAdaptivePacingRate(bool resetEnable)
{
  uint64_t win = std::max(m_tcb->m_cWnd.Get(), m_tcb->m_cWndInfl.Get());
  Time proposeSpread = m_tcb->m_lastRtt.Get();

  NS_LOG_INFO ("Proposed spreading cwnd " << win << " across " << 
                proposeSpread.GetSeconds() << "seconds");
  if (resetEnable)
    m_paced = true;

  double rate = (win / proposeSpread.GetSeconds()) * 8;
  // rate *= (1 + std::cbrt((double)m_tcb->m_segmentSize/win) + std::cbrt((double)m_tcb->m_segmentSize/(std::max(AvailableWindow () - m_tcb->m_segmentSize, (uint32_t)1))));
  // rate *= (1 + (double)BytesInFlight()/win);
  m_tcb->m_currentPacingRate = DataRate((uint64_t)rate);
  // 
  // m_tcb->m_currentPacingRate = DataRate((uint64_t)2 * m_rateNextRound);
  NS_LOG_INFO ("Updated pacing rate of subflow " << (int)m_subflowid << " to " << m_tcb->m_currentPacingRate);
}

void
TdTcpTxSubflow::SetGuard()
{
  m_guarded = true;
}

void
TdTcpTxSubflow::UnsetGuard()
{
  m_guarded = false;
}


}
