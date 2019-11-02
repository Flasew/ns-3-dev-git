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
#ifndef TCP_OPTION_TDTCP_H
#define TCP_OPTION_TDTCP_H

#include "tcp-option.h"
#include "tcp-header.h"
#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/sequence-number.h"
#include <vector>

namespace ns3 {

class TcpOptionTdTcpMain : public TcpOption 
{
public:
  /**
   * List the different subtypes of TDTCP options
   */
  enum SubType
  {
    TD_CAPABLE,
    TD_DSS,
    TD_CLOSE
  }

  TcpOptionTdTcp (void);
  virtual ~TcpOptionTdTcp (void);

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;

  static Ptr<TcpOption>
  CreateTdTcpOption (const uint8_t& kind);

  virtual uint32_t
  GetSerializedSize (void) const = 0;

  /** Get the subtype number assigned to this TDTCP option */
  virtual uint8_t
  GetKind (void) const;

  virtual void
  Serialize (Buffer::Iterator start) const = 0;

  /**
   * \return the TDTCP subtype of this class
   */
  virtual TcpOptionTdTcpMain::SubType
  GetSubType (void) const = 0;

protected:
  /**
   * \brief Serialize TCP option type & length of the option
   *
   * Let children write the subtype since Buffer iterators
   * can't write less than 1 byte
   * Should be called at the start of every subclass Serialize call
   */
  virtual void
  SerializeRef (Buffer::Iterator& i) const;

  /**
   * \brief Factorizes ation reading/subtype check that each subclass should do.
   * Should be called at the start of every subclass Deserialize
   * \return length of the option
   */
  uint32_t
  DeserializeRef (Buffer::Iterator& i) const;
};

  /**
   * \tparam SUBTYPE should be an integer
   */
template<TcpOptionTdTcpMain::SubType SUBTYPE>
class TcpOptionTdTcp : public TcpOptionTdTcpMain
{
public:
  TcpOptionTdTcp () : TcpOptionMpTcpMain ()
  {
  }

  virtual ~TcpOptionTdTcp (void)
  {
  }

  /**
   * \return TDTCP option type
   */
  virtual TcpOptionTcpMain::SubType
  GetSubType (void) const
  {
    return SUBTYPE;
  }
};

/**
 * \brief TD_CAPABLE used for connection handshake.
 * Unlike MPTCP which shakes for each subflow, TDTCP only shakes hands once
 * to establish the connection for all subflows. After all the concept of 
 * "subflow" is pretty abstract...
 *
 * HOST A                                              HOST B
 * ------                                              ------
 * SYN, TD_CAPABLE              ->                           
 * [A's # of (sender)subflows]                               
 *                              <-        SYN|ACK, TD_CAPABLE
 *                                        [B's # of subflows]
 * ACK, TD_CAPABLE              ->                           
 *
 * Header is fixed length (32 bits)
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +---------------+---------------+-------+-------+---------------+
 *  |     Kind      |    Length=4   |SubType|       |  # subflows   |
 *  +---------------+---------------+-------+-------+---------------+
 */
class TcpOptionTdTcpCapable : public TcpOptionTdTcp<TcpOptionTdTcpMain::TD_CAPABLE>
{
public:
  TcpOptionTdTcpCapable (void);
  virtual ~TcpOptionTdTcpCapable (void);

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  bool operator== (const TcpOptionTdTcpCapable&) const;

  virtual void SetNSubflows (const uint8_t& nSubflows);
  virtual uint64_t GetNSubflows (void) const;

  //! Inherited
  virtual void Print (std::ostream &os) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t GetSerializedSize (void) const;

protected:
  uint8_t m_nsubflows;

private:
  //! Defined and unimplemented to avoid misuse
  TcpOptionTdTcpCapable (const TcpOptionTdTcpCapable&);
  TcpOptionTdTcpCapable& operator= (const TcpOptionTdTcpCapable&);
};

/**
 * \brief TD_DSS used for Data transmission
 * Contains SEQ/ACK information of subflows. A data packet must contain at least
 * one of SEQ and ACK information.
 *
 * Header is variable length
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +---------------+---------------+-------+-------+---------------+
 *  |     Kind      |   Length=16   |SubType| |F|D|A|               |
 *  +---------------+---------------+-------+-------+---------------+
 *  | D seq subf ID | D car subf id | A seq subf ID | A car subf id |
 *  +---------------+---------------+-------+-------+---------------+
 *  |                          SSEQ (32 bits)                       |
 *  +---------------+---------------+-------+-------+---------------+
 *  |                          SACK (32 bits)                       |
 *  +---------------+---------------+-------+-------+---------------+
 *
 */ 

class TcpOptionTdTcpDSS : public TcpOptionTdTcp<TcpOptionTdTcpMain::TD_DSS>
{
public:
  
  static const uint8_t TD_FIN = 0x4;
  static const uint8_t TD_DATA = 0x2;
  // static const uint8_t TD_ACK = 0x1;

  TcpOptionTdTcpDSS (void);
  virtual ~TcpOptionTdTcpDSS (void);

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  bool operator== (const TcpOptionTdTcpDSS&) const;

  // virtual void SetFin ();
  virtual void SetData (const uint8_t& subflowId, const uint32_t & seq);
  virtual void SetAck (const uint8_t& subflowId, const uint32_t & ack);
  virtual void SetDataCarrier (const uint8_t& carrier);
  virtual void SetAckCarrier (const uint8_t& carrier);
  virtual bool GetData (uint8_t& subflowId, uint32_t & seq);
  virtual bool GetAck (uint8_t& subflowId, uint32_t & ack);
  virtual bool GetDataCarrier (uint8_t& carrier);
  virtual bool GetAckCarrier (uint8_t& carrier);

  //! Inherited
  virtual void Print (std::ostream &os) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t GetSerializedSize (void) const;

protected:
  bool m_hasdata;
  bool m_hasack;
  uint8_t m_dsubflowid;
  uint8_t m_asubflowid;
  uint8_t m_dcarrier;
  uint8_t m_acarrier;
  uint32_t m_dataseq;
  uint32_t m_acknum;

private:
  //! Defined and unimplemented to avoid misuse
  TcpOptionTdTcpDSS (const TcpOptionTdTcpDSS&);
  TcpOptionTdTcpDSS& operator= (const TcpOptionTdTcpDSS&);
};


/**
 * \brief TD_CLOSE used for fast close a connection
 * Analogue to MPTCP's FASTCLOSE option. Need more spec on what it does...
 *
 * Header is fixed length (32 bits)
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +---------------+---------------+-------+-------+---------------+
 *  |     Kind      |    Length     |SubType|       | Subflow num.  |
 *  +---------------+---------------+-------+-------+---------------+
 *  |                   LAST SSEQ of subflow num                    |
 *  +---------------+---------------+-------+-------+---------------+
 */ 
class TcpOptionTdTcpClose : public TcpOptionTdTcp<TcpOptionTdTcpMain::TD_CLOSE>
{
public:

  TcpOptionTdTcpClose (void);
  virtual ~TcpOptionTdTcpClose (void);

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  bool operator== (const TcpOptionTdTcpClose&) const;

  virtual void SetSubflowLastSSeq(const uint8_t &, const uint32_t &);
  virtual void GetSubflowLastSSeq(uint8_t &, uint32_t &);

  //! Inherited
  virtual void Print (std::ostream &os) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t GetSerializedSize (void) const;

protected:
  uint8_t m_subflowid;
  uint32_t m_lastseq;

private:
  //! Defined and unimplemented to avoid misuse
  TcpOptionTdTcpClose (const TcpOptionTdTcpClose&);
  TcpOptionTdTcpClose& operator= (const TcpOptionTdTcpClose&);
};


#endif // TCP_OPTION_TDTCP_H
