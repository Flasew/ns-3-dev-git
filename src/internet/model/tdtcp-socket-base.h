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

#include <vector>

namespace ns3 {

class TdTcpSocketBase : public TcpSocketBase {

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
   * \brief Called by the L3 protocol when it received a packet to pass on to TCP.
   *
   * \param packet the incoming packet
   * \param header the packet's IPv4 header
   * \param port the remote port
   * \param incomingInterface the incoming interface
   */
  virtual void ForwardUp (Ptr<Packet> packet, Ipv4Header header, uint16_t port, Ptr<Ipv4Interface> incomingInterface);

  /**
   * \brief Called by the L3 protocol when it received a packet to pass on to TCP.
   *
   * \param packet the incoming packet
   * \param header the packet's IPv6 header
   * \param port the remote port
   * \param incomingInterface the incoming interface
   */
  void ForwardUp6 (Ptr<Packet> packet, Ipv6Header header, uint16_t port, Ptr<Ipv6Interface> incomingInterface);

  /**
   * \brief Called by TcpSocketBase::ForwardUp{,6}().
   *
   * Get a packet from L3. This is the real function to handle the
   * incoming packet from lower layers. This is
   * wrapped by ForwardUp() so that this function can be overloaded by daughter
   * classes.
   *
   * \param packet the incoming packet
   * \param fromAddress the address of the sender of packet
   * \param toAddress the address of the receiver of packet (hopefully, us)
   */
  virtual void DoForwardUp (Ptr<Packet> packet, const Address &fromAddress,
                            const Address &toAddress);

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
   * \brief Extract at most maxSize bytes from the TxBuffer at sequence seq, add the
   *        TCP header, and send to TcpL4Protocol
   *
   * \param seq the sequence number
   * \param maxSize the maximum data block to be transmitted (in bytes)
   * \param withAck forces an ACK to be sent
   * \returns the number of bytes sent
   */
  virtual uint32_t SendDataPacket (SequenceNumber32 seq, uint32_t maxSize, bool withAck);

  /**
   * \brief Send a empty packet that carries a flag, e.g., ACK
   *
   * \param flags the packet's flags
   */
  virtual void SendEmptyPacket (uint8_t flags);
  /**
   * \brief
   * \param header A valid TCP header
   * \param p Packet to send. May be empty.
   */
  virtual void SendPacket(TcpHeader header, Ptr<Packet> p);

  /**
   * \brief Send reset and tear down this socket
   */
  virtual void SendRST (void);

  /**
   * \brief Check if a sequence number range is within the rx window
   *
   * \param head start of the Sequence window
   * \param tail end of the Sequence window
   * \returns true if it is in range
   */
  virtual bool OutOfRange (SequenceNumber32 head, SequenceNumber32 tail) const;


  // Helper functions: Connection close

  /**
   * \brief Close a socket by sending RST, FIN, or FIN+ACK, depend on the current state
   *
   * \returns 0 on success
   */
  virtual int DoClose (void);

  /**
   * \brief Peacefully close the socket by notifying the upper layer and deallocate end point
   */
  virtual void CloseAndNotify (void);

  /**
   * \brief Kill this socket by zeroing its attributes (IPv4)
   *
   * This is a callback function configured to m_endpoint in
   * SetupCallback(), invoked when the endpoint is destroyed.
   */
  virtual void Destroy (void);

  /**
   * \brief Kill this socket by zeroing its attributes (IPv6)
   *
   * This is a callback function configured to m_endpoint in
   * SetupCallback(), invoked when the endpoint is destroyed.
   */
  virtual void Destroy6 (void);

  /**
   * \brief Deallocate m_endPoint and m_endPoint6
   */
  virtual void DeallocateEndPoint (void);

  /**
   * \brief Received a FIN from peer, notify rx buffer
   *
   * \param p the packet
   * \param tcpHeader the packet's TCP header
   */
  virtual void PeerClose (Ptr<Packet> p, const TcpHeader& tcpHeader);

  /**
   * \brief FIN is in sequence, notify app and respond with a FIN
   */
  virtual void DoPeerClose (void);

  /**
   * \brief Cancel all timer when endpoint is deleted
   */
  virtual void CancelAllTimers (void);

  /**
   * \brief Move from CLOSING or FIN_WAIT_2 to TIME_WAIT state
   */
  virtual void TimeWait (void);

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
   * \brief Received a packet upon LISTEN state.
   *
   * \param packet the packet
   * \param tcpHeader the packet's TCP header
   * \param fromAddress the source address
   * \param toAddress the destination address
   */
  virtual void ProcessListen (Ptr<Packet> packet, const TcpHeader& tcpHeader,
                      const Address& fromAddress, const Address& toAddress);

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

  /**
   * \brief Received a packet upon CLOSE_WAIT, FIN_WAIT_1, FIN_WAIT_2
   *
   * \param packet the packet
   * \param tcpHeader the packet's TCP header
   */
  virtual void ProcessWait (Ptr<Packet> packet, const TcpHeader& tcpHeader);

  /**
   * \brief Received a packet upon CLOSING
   *
   * \param packet the packet
   * \param tcpHeader the packet's TCP header
   */
 virtual void ProcessClosing (Ptr<Packet> packet, const TcpHeader& tcpHeader);

  /**
   * \brief Received a packet upon LAST_ACK
   *
   * \param packet the packet
   * \param tcpHeader the packet's TCP header
   */
  virtual void ProcessLastAck (Ptr<Packet> packet, const TcpHeader& tcpHeader);

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
   * \brief Return total bytes in flight
   *
   * Does not count segments lost and SACKed (or dupACKed)
   *
   * \returns total bytes in flight
   */
  virtual uint32_t BytesInFlight (void) const;

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

  /**
   * \brief Process a received ack
   * \param ackNumber ack number
   * \param scoreboardUpdated if true indicates that the scoreboard has been
   * \param oldHeadSequence value of HeadSequence before ack
   * updated with SACK information
   */
  virtual void ProcessAck (const SequenceNumber32 &ackNumber, bool scoreboardUpdated,
                           const SequenceNumber32 &oldHeadSequence);

  /**
   * \brief Recv of a data, put into buffer, call L7 to get it if necessary
   * \param packet the packet
   * \param tcpHeader the packet's TCP header
   */
  virtual void ReceivedData (Ptr<Packet> packet, const TcpHeader& tcpHeader);

  /**
   * \brief Take into account the packet for RTT estimation
   * \param tcpHeader the packet's TCP header
   */
  virtual void EstimateRtt (const TcpHeader& tcpHeader);

  /**
   * \brief Update the RTT history, when we send TCP segments
   *
   * \param seq The sequence number of the TCP segment
   * \param sz The segment's size
   * \param isRetransmission Whether or not the segment is a retransmission
   */

  virtual void UpdateRttHistory (const SequenceNumber32 &seq, uint32_t sz,
                                 bool isRetransmission);

  /**
   * \brief Update buffers w.r.t. ACK
   * \param seq the sequence number
   * \param resetRTO indicates if RTO should be reset
   */
  virtual void NewAck (SequenceNumber32 const& seq, bool resetRTO);

  /**
   * \brief Dupack management
   */
  void DupAck ();

  /**
   * \brief Enter the CA_RECOVERY, and retransmit the head
   */
  void EnterRecovery ();

  /**
   * \brief An RTO event happened
   */
  virtual void ReTxTimeout (void);

  /**
   * \brief Action upon delay ACK timeout, i.e. send an ACK
   */
  virtual void DelAckTimeout (void);

  /**
   * \brief Timeout at LAST_ACK, close the connection
   */
  virtual void LastAckTimeout (void);

  /**
   * \brief Send 1 byte probe to get an updated window size
   */
  virtual void PersistTimeout (void);

  /**
   * \brief Retransmit the first segment marked as lost, without considering
   * available window nor pacing.
   */
  virtual void DoRetransmit (void);

  /** \brief Add options to TcpHeader
   *
   * Test each option, and if it is enabled on our side, add it
   * to the header
   *
   * \param tcpHeader TcpHeader to add options to
   */
  virtual void AddOptions (TcpHeader& tcpHeader);

  /**
   * This function first generates a copy of the current socket as an MpTcpSubflow.
   * Then it upgrades the current socket to an MpTcpSocketBase via the use of
   * "placement new", i.e. it does not allocate new memory but reuse the memory at "this"
   * address to instantiate MpTcpSocketBase.
   * Finally the master socket is associated to the meta.
   *
   * It is critical that enough memory was allocated beforehand to contain MpTcpSocketBase
   * (see how it's done for now in TcpL4Protocol).
   * Ideally MpTcoSocketBase would take less memory than TcpSocketBase, so one of the goal should be to let
   * MpTcpSocketBase inherit directly from TcpSocket rather than TcpSocketBase.
   *
   * The function does not register the new subflow in m_tcp->AddSocket, this should be taken care
   * of afterwards.
   *
   * \param master
   * \return master subflow. It is not associated to the meta at this point
   */
  virtual Ptr<MpTcpSubflow> UpgradeToMeta();

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
   * \brief Return true if the specified option is enabled
   *
   * \param kind kind of TCP option
   * \return true if the option is enabled
   */
  virtual bool IsTcpOptionEnabled (uint8_t kind) const;

  /**
   * \brief Read and parse the Window scale option
   *
   * Read the window scale option (encoded logarithmically) and save it.
   * Per RFC 1323, the value can't exceed 14.
   *
   * \param option Window scale option read from the header
   */
  virtual void ProcessOptionWScale (const Ptr<const TcpOption> option);
  /**
   * \brief Add the window scale option to the header
   *
   * Calculate our factor from the rxBuffer max size, and add it
   * to the header.
   *
   * \param header TcpHeader where the method should add the window scale option
   */
  virtual void AddOptionWScale (TcpHeader& header);

  /**
   * \brief Add the mptcp option to the header
   * \param header TcpHeader where the method should add mptcp option
   */
  virtual void AddMpTcpOptions (TcpHeader& header);

  /**
   * \brief Calculate window scale value based on receive buffer space
   *
   * Calculate our factor from the rxBuffer max size
   *
   * \returns the Window Scale factor
   */
  uint8_t CalculateWScale () const;

  /**
   * \brief Read the SACK PERMITTED option
   *
   * Currently this is a placeholder, since no operations should be done
   * on such option.
   *
   * \param option SACK PERMITTED option from the header
   */
  void ProcessOptionSackPermitted (const Ptr<const TcpOption> option);

  /**
   * \brief Read the SACK option
   *
   * \param option SACK option from the header
   * \returns true in case of an update to the SACKed blocks
   */
  bool ProcessOptionSack (const Ptr<const TcpOption> option);

  /**
   * \brief Add the SACK PERMITTED option to the header
   *
   * \param header TcpHeader where the method should add the option
   */
  void AddOptionSackPermitted (TcpHeader &header);

  /**
   * \brief Add the SACK option to the header
   *
   * \param header TcpHeader where the method should add the option
   */
  void AddOptionSack (TcpHeader& header);

  /** \brief Process the timestamp option from other side
   *
   * Get the timestamp and the echo, then save timestamp (which will
   * be the echo value in our out-packets) and save the echoed timestamp,
   * to utilize later to calculate RTT.
   *
   * \see EstimateRtt
   * \param option Option from the segment
   * \param seq Sequence number of the segment
   */
  void ProcessOptionTimestamp (const Ptr<const TcpOption> option,
                               const SequenceNumber32 &seq);
  /**
   * \brief Add the timestamp option to the header
   *
   * Set the timestamp as the lower bits of the Simulator::Now time,
   * and the echo value as the last seen timestamp from the other part.
   *
   * \param header TcpHeader to which add the option to
   */
  void AddOptionTimestamp (TcpHeader& header);

  /**
   * \brief Performs a safe subtraction between a and b (a-b)
   *
   * Safe is used to indicate that, if b>a, the results returned is 0.
   *
   * \param a first number
   * \param b second number
   * \return 0 if b>0, (a-b) otherwise
   */
  static uint32_t SafeSubtraction (uint32_t a, uint32_t b);

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
  void ProcessOptionTdTcp();

private:
  std::vector<Ptr<TdTcpTxSubflow>> m_txsubflows; // send packet and eats ack
  std::vector<Ptr<TdTcpRxSubflow>> m_rxsubflows; // eats data and send ack
  uint16_t m_currTxSubflow;

};


}

#endif // TDTCP_SOCKET_BASE_H