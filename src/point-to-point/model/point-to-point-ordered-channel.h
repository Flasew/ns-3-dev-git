/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 * Author: Weiyang Wang <wew168@ucsd.edu>
 */

// A re-implementation of point to point channel that doesn't reorder packets

#ifndef POINT_TO_POINT_ORDERED_CHANNEL_H
#define POINT_TO_POINT_ORDERED_CHANNEL_H

#include <list>

#include "point-to-point-channel.h"

namespace ns3 {

/**
 * \ingroup point-to-point
 *
 * \brief A Ordered Point-To-Point Channel
 * 
 * This object connects two point-to-point net devices where at least one
 * is not local to this simulator object. It simply override the transmit
 * method and uses an MPI Send operation instead.
 */
class PointToPointOrderedChannel : public PointToPointChannel
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /** 
   * \brief Constructor
   */
  PointToPointOrderedChannel ();

  /** 
   * \brief Deconstructor
   */
  ~PointToPointOrderedChannel ();

  /**
   * \brief Transmit the packet
   *
   * \param p Packet to transmit
   * \param src Source PointToPointNetDevice
   * \param txTime Transmit time to apply
   * \returns true if successful (currently always true)
   */
  virtual bool TransmitStart (Ptr<const Packet> p, Ptr<PointToPointNetDevice> src,
                              Time txTime);

private:

  /**
   * \brief method that actually dispatches the packet 
   *
   * \param p Packet to transmit
   * \param src Source PointToPointNetDevice
   */
  void PacketGo(uint32_t wire);

  std::list<Ptr<Packet>> * m_packetQueues;
};

} // namespace ns3

#endif


