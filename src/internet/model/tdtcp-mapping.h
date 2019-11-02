/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 University of Sussex
 * Copyright (c) 2015 Université Pierre et Marie Curie (UPMC)
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
 * Author:  Kashif Nadeem <kshfnadeem@gmail.com>
 *          Matthieu Coudron <matthieu.coudron@lip6.fr>
 *          Morteza Kheirkhah <m.kheirkhah@sussex.ac.uk>
 *          Weiyang Wang <wew168@ucsd.edu>
 *
 */
#ifndef TDTCP_MAPPING_H
#define TDTCP_MAPPING_H

#include <stdint.h>
#include <vector>
#include <queue>
#include <list>
#include <set>
#include <map>
#include "ns3/object.h"
#include "ns3/uinteger.h"
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/sequence-number.h"
#include "ns3/rtt-estimator.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"
#include "ns3/tcp-socket.h"
#include "ns3/ipv4-address.h"
#include "ns3/tcp-tx-buffer.h"
#include "ns3/tcp-rx-buffer.h"

namespace ns3
{
/**
 *\brief A base class for implementation of mapping for tdtcp.
 *
 * Just copied everything from MpTcpMapping and change all SEQ64 to SEQ32, 
 * since TDTCP uses the over all SEQ for data sequence, which is limited to
 * 32bit. 
 *
 * Let's hope some other TCP2.0 have a better sequence number.
 */

class TdTcpMapping
{
public:
  TdTcpMapping(void);
  virtual ~TdTcpMapping(void);

  /**
   * \brief Set subflow sequence number
   * \param headSSN
   */
  void MapToSSN( SequenceNumber32 const& headSSN);

  /**
   * \return True if mappings share DSN space
   * \param headSSN head SSN
   * \param len 
   */

  virtual bool
  OverlapRangeSSN(const SequenceNumber32& headSSN, const uint16_t& len) const;

  virtual bool
  OverlapRangeDSN(const SequenceNumber32& headDSN, const uint16_t& len) const;
  void SetHeadDSN(SequenceNumber32 const&);

  /**
   * \brief Set mapping length
   */
  virtual void
  SetMappingSize(uint16_t const&);

  /**
   * \param ssn Data seqNb
   */
  bool IsSSNInRange(SequenceNumber32 const& ssn) const;

  /**
   * \param dsn Data seqNb
   */
  bool IsDSNInRange(SequenceNumber32 const& dsn) const;

  /**
   * \param ssn Subflow sequence number
   * \param dsn Data Sequence Number
   * \return True if ssn belonged to this mapping, then a dsn would have been computed
   *
   */
  bool
  TranslateSSNToDSN(const SequenceNumber32& ssn, SequenceNumber32& dsn) const;

  /**
   * \return The last TDTCP sequence number included in the mapping
   */
  SequenceNumber32 TailDSN (void) const;

  /**
   * \return The last subflow sequence number included in the mapping
   */
  SequenceNumber32 TailSSN (void) const;

  /**
   * \brief Necessary for
   * \brief std::set to sort mappings
   * Compares ssn
   * \brief Compares mapping based on their DSN number. It is required when inserting into a set
   */
  bool operator<(TdTcpMapping const& ) const;

  /**
   * \return TDTCP sequence number for the first mapped byte
   */
  virtual SequenceNumber32
  HeadDSN() const;

  /**
   * \return subflow sequence number for the first mapped byte
   */
  virtual SequenceNumber32
  HeadSSN() const;

  /**
   * \return mapping length
   */
  virtual uint16_t
  GetLength() const ;

  /**
   * \brief Mapping are equal if everything concord, SSN/DSN and length
   */
  virtual bool operator==( const TdTcpMapping&) const;

  /**
   * \return Not ==
   */
  virtual bool operator!=( const TdTcpMapping& mapping) const;

protected:
  SequenceNumber32 m_dataSequenceNumber;   //!< TDTCP sequence number
  SequenceNumber32 m_subflowSequenceNumber;  //!< subflow sequence number
  uint16_t m_dataLevelLength;  //!< mapping length / size
};

/**
 * \brief Depending on modifications allowed in upstream ns3, it may some day inherit from TcpTxbuffer etc ...
 * \brief Meanwhile we have a pointer towards the buffers.
 *
 * \class TdTcpMappingContainer
 * Mapping handling
 * Once a mapping has been advertised on a subflow, it must be honored. If the remote host already received the data
 * (because it was sent in parallel over another subflow), then the received data must be discarded.
 * Could be fun implemented as an interval tree
 * http://www.geeksforgeeks.org/interval-tree/
 */

class TdTcpMappingContainer
{
public:
  TdTcpMappingContainer(void);
  virtual ~TdTcpMappingContainer(void);

  /**
   * \brief Discard mappings which TailDSN() < maxDsn and TailSSN() < maxSSN
   *
   * This can be called only when dsn is in the meta socket Rx buffer and in order
   * (since it may renegate some data when out of order).
   * The mapping should also have been thoroughly fulfilled at the subflow level.
   * \return Number of mappings discarded. >= 0
   */

  /**
   * \brief When Buffers work in non renegotiable mode,
   * it should be possible to remove them one by one
   * \param mapping to be discarded
   */
  bool DiscardMapping(const TdTcpMapping& mapping);

  /**
   * \param firstUnmappedSsn last mapped SSN.
   * \return true if non empty
   */
  bool FirstUnmappedSSN(SequenceNumber32& firstUnmappedSsn) const;

  /**
   * \brief For debug purpose. Dump all registered mappings
   */
  virtual void Dump() const;

  /**
   * \brief
   * Should do no check
   * The mapping
   * \note Check for overlap.
   * \return False if the dsn range overlaps with a registered mapping, true otherwise
  **/
  bool AddMapping(const TdTcpMapping& mapping);

  /**
   * \param l list
   * \param m pass on the mapping you want to retrieve
   */
  bool
  GetMappingForSSN(const SequenceNumber32& ssn, TdTcpMapping& m) const;

  /**
   * \param dsn
   */
  virtual bool GetMappingsStartingFromSSN(SequenceNumber32 ssn, std::set<TdTcpMapping>& mappings);

protected:

    std::set<TdTcpMapping> m_mappings;     //!< it is a set ordered by SSN
};

std::ostream& operator<<(std::ostream &os, const TdTcpMapping& mapping);

} //namespace ns3
#endif //MP_TCP_TYPEDEFS_H
