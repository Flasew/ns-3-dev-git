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

#ifndef TDTCP_SOCKET_BASE_H
#define TDTCP_SOCKET_BASE_H


#include "ns3/callback.h"
#include "ns3/tcp-socket.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/inet-socket-address.h"

#include <map>
#include <vector>

namespace ns3 {

class Ipv4EndPoint;
class Ipv6EndPoint;
class Node;
class Packet;
class TcpL4Protocol;
class TdTcpTxSubflow;
class TdTcpRxSubflow;
class TcpOptionTDCapable;
class TcpOptionTDDSS;
class TcpOptionTDClose;
class OutputStreamWrapper;
class TcpSocketBase;


class TdTcpSocketBase : public TcpSocketBase {

  friend class TdTcpTxSubflow;
  friend class TdTcpRxSubflow;

public:

  // NS3 object management
  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId (void) const;

  // Constructors
  TdTcpSocketBase();

  TdTcpSocketBase(const TcpSocketBase& sock);
  TdTcpSocketBase(const TdTcpSocketBase&);
  virtual ~TdTcpSocketBase();

protected:

  // Helper functions: Connection set up

  /**
   * \brief Common part of the two Bind(), i.e. set callback and remembering local addr:port
   *
   * \returns 0 on success, -1 on failure
   */
  virtual int SetupCallback (void);

  /**
   * \brief Perform the real connection tasks: Send SYN if allowed, RST if invalid
   *
   * \returns 0 on success
   */
  virtual int DoConnect (void);

  /**
   * \brief Schedule-friendly wrapper for Socket::NotifyConnectionSucceeded()
   */
  virtual void ConnectionSucceeded (void);

  /**
   * \brief Configure the endpoint to a local address. Called by Connect() if Bind() didn't specify one.
   *
   * \returns 0 on success
   */
  int SetupEndpoint (void);

  /**
   * \brief Configure the endpoint v6 to a local address. Called by Connect() if Bind() didn't specify one.
   *
   * \returns 0 on success
   */
  int SetupEndpoint6 (void);

  /**
   * \brief Complete a connection by forking the socket
   *
   * This function is called only if a SYN received in LISTEN state. After
   * TcpSocketBase cloned, allocate a new end point to handle the incoming
   * connection and send a SYN+ACK to complete the handshake.
   *
   * \param p the packet triggering the fork
   * \param tcpHeader the TCP header of the triggering packet
   * \param fromAddress the address of the remote host
   * \param toAddress the address the connection is directed to
   */
  virtual void CompleteFork (Ptr<Packet> p, const TcpHeader& tcpHeader,
                             const Address& fromAddress, const Address& toAddress);



  // Helper functions: Transfer operation

  /**
   * \brief Checks whether the given TCP segment is valid or not.
   *
   * \param seq the sequence number of packet's TCP header
   * \param tcpHeaderSize the size of packet's TCP header
   * \param tcpPayloadSize the size of TCP payload
   */
  bool IsValidTcpSegment (const SequenceNumber32 seq, const uint32_t tcpHeaderSize,
                          const uint32_t tcpPayloadSize);

  /**
   * \brief Called by the L3 protocol when it received an ICMP packet to pass on to TCP.
   *
   * \param icmpSource the ICMP source address
   * \param icmpTtl the ICMP Time to Live
   * \param icmpType the ICMP Type
   * \param icmpCode the ICMP Code
   * \param icmpInfo the ICMP Info
   */
  void ForwardIcmp (Ipv4Address icmpSource, uint8_t icmpTtl, uint8_t icmpType, uint8_t icmpCode, uint32_t icmpInfo);

  /**
   * \brief Called by the L3 protocol when it received an ICMPv6 packet to pass on to TCP.
   *
   * \param icmpSource the ICMP source address
   * \param icmpTtl the ICMP Time to Live
   * \param icmpType the ICMP Type
   * \param icmpCode the ICMP Code
   * \param icmpInfo the ICMP Info
   */
  void ForwardIcmp6 (Ipv6Address icmpSource, uint8_t icmpTtl, uint8_t icmpType, uint8_t icmpCode, uint32_t icmpInfo);

  /**
   * \brief Send as much pending data as possible according to the Tx window.
   *
   * Note that this function did not implement the PSH flag.
   *
   * \param withAck forces an ACK to be sent
   * \returns the number of packets sent
   */
  virtual uint32_t SendPendingData (bool withAck = false);

  /**
   * \brief Check if a sequence number range is within the rx window
   *
   * \param head start of the Sequence window
   * \param tail end of the Sequence window
   * \returns true if it is in range
   */
  virtual bool OutOfRange (SequenceNumber32 head, SequenceNumber32 tail) const;


  // State transition functions

  /**
   * \brief Received a packet upon ESTABLISHED state.
   *
   * This function is mimicking the role of tcp_rcv_established() in tcp_input.c in Linux kernel.
   *
   * \param packet the packet
   * \param tcpHeader the packet's TCP header
   */
  virtual void ProcessEstablished (Ptr<Packet> packet, const TcpHeader& tcpHeader); // Received a packet upon ESTABLISHED state

  /**
   * \brief Received a packet upon SYN_SENT
   *
   * \param packet the packet
   * \param tcpHeader the packet's TCP header
   */
  virtual void ProcessSynSent (Ptr<Packet> packet, const TcpHeader& tcpHeader);

  /**
   * \brief Received a packet upon SYN_RCVD.
   *
   * \param packet the packet
   * \param tcpHeader the packet's TCP header
   * \param fromAddress the source address
   * \param toAddress the destination address
   */
  virtual void ProcessSynRcvd (Ptr<Packet> packet, const TcpHeader& tcpHeader,
                       const Address& fromAddress, const Address& toAddress);

  // Window management

  /**
   * \brief Return count of number of unacked bytes
   *
   * The difference between SND.UNA and HighTx
   *
   * \returns count of number of unacked bytes
   */
  virtual uint32_t UnAckDataCount (void) const;

  /**
   * \brief Return the max possible number of unacked bytes
   * \returns the max possible number of unacked bytes
   */
  virtual uint32_t Window (void) const;

  /**
   * \brief Return unfilled portion of window
   * \return unfilled portion of window
   */
  virtual uint32_t AvailableWindow (void) const;

  /**
   * \brief The amount of Rx window announced to the peer
   * \param scale indicate if the window should be scaled. True for
   * almost all cases, except when we are sending a SYN
   * \returns size of Rx window announced to the peer
   */
  virtual uint16_t AdvertisedWindowSize (bool scale = true) const;

  /**
   * \brief Update the receiver window (RWND) based on the value of the
   * window field in the header.
   *
   * This method suppresses updates unless one of the following three
   * conditions holds:  1) segment contains new data (advancing the right
   * edge of the receive buffer), 2) segment does not contain new data
   * but the segment acks new data (highest sequence number acked advances),
   * or 3) the advertised window is larger than the current send window
   *
   * \param header TcpHeader from which to extract the new window value
   */
  virtual bool UpdateWindowSize (const TcpHeader& header);

  // Manage data tx/rx

  /**
   * \brief Call CopyObject<> to clone me
   * \returns a copy of the socket
   */
  virtual Ptr<TcpSocketBase> Fork (void);

  /**
   * \brief Received an ACK packet
   * \param packet the packet
   * \param tcpHeader the packet's TCP header
   */
  virtual void ReceivedAck (Ptr<Packet> packet, const TcpHeader& tcpHeader);


  /** \brief Add options to TcpHeader
   *
   * Test each option, and if it is enabled on our side, add it
   * to the header
   *
   * \param tcpHeader TcpHeader to add options to
   */
  virtual void AddOptions (TcpHeader& tcpHeader);


  /**
   * \return if it returns 1, we need to upgrade the meta socket
   * if negative then it should discard the packet ?
   */
  virtual int ProcessTcpOptions(const TcpHeader& header);

  /**
   * \brief In this baseclass, this only deals with MpTcpCapable options in order to know if the socket
   * should be converted to an MPTCP meta socket.
   */
  virtual int ProcessOptionTdTcp(const Ptr<const TcpOption> option);

  /**
   * \brief Read TCP options before Ack processing
   *
   * Timestamp and Window scale are managed in other pieces of code.
   *
   * \param tcpHeader Header of the segment
   * \param scoreboardUpdated indicates if the scoreboard was updated due to a
   * SACK option
   */
  virtual void ReadOptions (const TcpHeader &tcpHeader, bool &scoreboardUpdated);

  /**
   * \brief Notify Pacing
   */
  void NotifyPacingPerformed (void);

  /**
   * \brief Add Tags for the Socket
   * \param p Packet
   */
  void AddSocketTags (const Ptr<Packet> &p) const;

  // Added functions. Not categorized for now...

  // Process Option
  void ProcessOptionTdTcp (const Ptr<const TcpOption> & option);

  uint8_t ProcessOptionTdSYN (const Ptr<const TcpOption> & option);

  bool ProcessOptionTdDSS (const Ptr<const TcpOption> & option, 
                           int16_t & ssubflow,
                           int16_t & rsubflow);

  bool ProcessOptionTdDSS (const Ptr<const TcpOption> & option,
                           int16_t & ssubflow,
                           uint32_t & sseq,
                           int16_t & rsubflow,
                           uint32_t & sack);

  // Add option
  void AddOptionTDSYN(TcpHeader &header, uint8_t n);

  void AddOptionTDDSS(TcpHeader &header,
                                bool data,
                                uint8_t ssid,
                                uint8_t csid, 
                                uint32_t sseq, 
                                bool ack, 
                                uint8_t said, 
                                uint8_t caid, 
                                uint32_t sack);

  // Deliver Data
  void DeliverDataToSubflow(uint8_t sid,
                            uint8_t cid,
                            uint32_t sseq,
                            Ptr<Packet> packet, 
                            const TcpHeader& tcpHeader);

  // Deliver ACK
  void DeliverAckToSubflow(uint8_t asid,
                           uint8_t acid,
                           Ptr<Packet> packet, 
                           const TcpHeader& tcpHeader);

  // Received Data
  void OnSubflowReceive (Ptr<Packet> packet, 
                               const TcpHeader& tcpHeader, 
                               Ptr<TdTcpRxSubflow> sf,
                               uint32_t sack);

  // Send Special packet 
  void SendSYN(bool withAck = false);
  void SendHandShakeACK ();
  void SendAckPacket (uint8_t subflowid, uint8_t scid, uint32_t sack);

  // change active subflow
  void ChangeActivateSubflow(uint8_t newsid);

private:
  std::vector<Ptr<TdTcpTxSubflow>> m_txsubflows; // send packet and eats ack
  std::vector<Ptr<TdTcpRxSubflow>> m_rxsubflows; // eats data and send ack
  uint8_t m_currTxSubflow;

  uint32_t m_connDupAckTh {50};
  std::map<SequenceNumber32, Ptr<TdTcpTxSubflow>> m_seqToSubflowMap;


};


}

#endif // TDTCP_SOCKET_BASE_H