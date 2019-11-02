
// Received packet from L3
// matches the packet to the proper rxsubflow 

// void
// TdTcpSocketBase::DoForwardUp (Ptr<Packet> packet, const Address &fromAddress,
//                             const Address &toAddress) 
// {
//   // in case the packet still has a priority tag attached, remove it
//   SocketPriorityTag priorityTag;
//   packet->RemovePacketTag (priorityTag);

//   // Peel off TCP header
//   TcpHeader tcpHeader;
//   packet->RemoveHeader (tcpHeader);
//   SequenceNumber32 seq = tcpHeader.GetSequenceNumber ();

//   m_rxTrace (packet, tcpHeader, this);

//   if (tcpHeader.GetFlags () & TcpHeader::SYN) {
//     /* The window field in a segment where the SYN bit is set (i.e., a <SYN>
//      * or <SYN,ACK>) MUST NOT be scaled (from RFC 7323 page 9). But should be
//      * saved anyway..
//      */
//     m_rWnd = tcpHeader.GetWindowSize ();

//     if (tcpHeader.HasOption (TcpOption::WINSCALE) && m_winScalingEnabled) {
//       ProcessOptionWScale (tcpHeader.GetOption (TcpOption::WINSCALE));
//     }
//     else {
//       m_winScalingEnabled = false;
//     }

//     if (tcpHeader.HasOption (TcpOption::SACKPERMITTED) && m_sackEnabled) {
//       ProcessOptionSackPermitted (tcpHeader.GetOption (TcpOption::SACKPERMITTED));
//     }
//     else {
//       m_sackEnabled = false;
//     }

//     // When receiving a <SYN> or <SYN-ACK> we should adapt TS to the other end
//     if (tcpHeader.HasOption (TcpOption::TS) && m_timestampEnabled) {
//       ProcessOptionTimestamp (tcpHeader.GetOption (TcpOption::TS),
//                                tcpHeader.GetSequenceNumber ());
//     }
//     else {
//       m_timestampEnabled = false;
//     }

//     if (tcpHeader.HasOption (TcpOption::TDTCP) && m_tdtcpEnabled) {
//       ProcessOptionTdTcp(tcpHeader.GetOption(TcpOption::TDTCP));
//     }
//     else {
//       NS_FATAL_ERROR("Don't use TDTCP socket for regular TCP for now...");
//     }

//     // // Initialize cWnd and ssThresh
//     // m_tcb->m_cWnd = GetInitialCwnd () * GetSegSize ();
//     // m_tcb->m_cWndInfl = m_tcb->m_cWnd;
//     // m_tcb->m_ssThresh = GetInitialSSThresh ();

//     if (tcpHeader.GetFlags () & TcpHeader::ACK) {
//       EstimateRtt (tcpHeader);
//       m_highRxAckMark = tcpHeader.GetAckNumber ();
//     }
//   }

//   else if (tcpHeader.GetFlags () & TcpHeader::ACK)
//   {
//     NS_ASSERT (!(tcpHeader.GetFlags () & TcpHeader::SYN));
//     if (m_timestampEnabled)
//     {
//       if (!tcpHeader.HasOption (TcpOption::TS))
//       {
//         // Ignoring segment without TS, RFC 7323
//         NS_LOG_LOGIC ("At state " << TcpStateName[m_state] <<
//                       " received packet of seq [" << seq <<
//                       ":" << seq + packet->GetSize () <<
//                       ") without TS option. Silently discard it");
//         return;
//       }
//       else
//       {
//         ProcessOptionTimestamp (tcpHeader.GetOption (TcpOption::TS),
//                                 tcpHeader.GetSequenceNumber ());
//       }
//     }

//     EstimateRtt (tcpHeader);
//     UpdateWindowSize (tcpHeader);
//   }


//   if (m_rWnd.Get () == 0 && m_persistEvent.IsExpired ())
//   { // Zero window: Enter persist state to send 1 byte to probe
//     NS_LOG_LOGIC (this << " Enter zerowindow persist state");
//     NS_LOG_LOGIC (this << " Cancelled ReTxTimeout event which was set to expire at " <<
//                   (Simulator::Now () + Simulator::GetDelayLeft (m_retxEvent)).GetSeconds ());
//     m_retxEvent.Cancel ();
//     NS_LOG_LOGIC ("Schedule persist timeout at time " <<
//                   Simulator::Now ().GetSeconds () << " to expire at time " <<
//                   (Simulator::Now () + m_persistTimeout).GetSeconds ());
//     m_persistEvent = Simulator::Schedule (m_persistTimeout, &TcpSocketBase::PersistTimeout, this);
//     NS_ASSERT (m_persistTimeout == Simulator::GetDelayLeft (m_persistEvent));
//   }

//   // TCP state machine code in different process functions
//   // C.f.: tcp_rcv_state_process() in tcp_input.c in Linux kernel
//   switch (m_state)
//   {
//   case ESTABLISHED:
//     ProcessEstablished (packet, tcpHeader);
//     break;
//   case LISTEN:
//     ProcessListen (packet, tcpHeader, fromAddress, toAddress);
//     break;
//   case TIME_WAIT:
//     // Do nothing
//     break;
//   case CLOSED:
//     // Send RST if the incoming packet is not a RST
//     if ((tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG)) != TcpHeader::RST)
//     { // Since m_endPoint is not configured yet, we cannot use SendRST here
//       TcpHeader h;
//       Ptr<Packet> p = Create<Packet> ();
//       h.SetFlags (TcpHeader::RST);
//       h.SetSequenceNumber (m_tcb->m_nextTxSequence);
//       h.SetAckNumber (m_rxBuffer->NextRxSequence ());
//       h.SetSourcePort (tcpHeader.GetDestinationPort ());
//       h.SetDestinationPort (tcpHeader.GetSourcePort ());
//       h.SetWindowSize (AdvertisedWindowSize ());
//       if (!m_mptcpEnabled)
//       {
//         AddOptions (h);
//       }
//       m_txTrace (p, h, this);
//       if (m_mptcpEnabled)
//       {
//          SendPacket(h,p);
//       }
//       else
//       {
//         m_tcp->SendPacket (p, h, toAddress, fromAddress, m_boundnetdevice);
//       }
//     }
//     break;
//   case SYN_SENT:
//     ProcessSynSent (packet, tcpHeader);
//     break;
//   case SYN_RCVD:
//     ProcessSynRcvd (packet, tcpHeader, fromAddress, toAddress);
//     break;
//   case FIN_WAIT_1:
//   case FIN_WAIT_2:
//   case CLOSE_WAIT:
//     ProcessWait (packet, tcpHeader);
//     break;
//   case CLOSING:
//     ProcessClosing (packet, tcpHeader);
//     break;
//   case LAST_ACK:
//     ProcessLastAck (packet, tcpHeader);
//     break;
//   default: // mute compiler
//     break;
//   }

//   if (m_rWnd.Get () != 0 && m_persistEvent.IsRunning ())
//   { // persist probes end, the other end has increased the window
//     NS_ASSERT (m_connected);
//     NS_LOG_LOGIC (this << " Leaving zerowindow persist state");
//     m_persistEvent.Cancel ();

//     SendPendingData (m_connected);
//   }
// }

// CONNECTION ESTABLISHMENT
// void 
// TdTcpSocketBase::ProcessListen (Ptr<Packet> packet, const TcpHeader& tcpHeader,
//                       const Address& fromAddress, const Address& toAddress) 
// {
//   NS_LOG_FUNCTION (this << tcpHeader);

//   // Extract the flags. PSH, URG, CWR and ECE are disregarded.
//   uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG | TcpHeader::CWR | TcpHeader::ECE);

//   // Fork a socket if received a SYN. Do nothing otherwise.
//   // C.f.: the LISTEN part in tcp_v4_do_rcv() in tcp_ipv4.c in Linux kernel
//   if (tcpflags != TcpHeader::SYN)
//   {
//     return;
//   }

//   // Call socket's notify function to let the server app know we got a SYN
//   // If the server app refuses the connection, do nothing
//   if (!NotifyConnectionRequest (fromAddress))
//   {
//     return;
//   }
// }