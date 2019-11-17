/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019, University of California
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
 */

#ifndef POINT_TO_POINT_TDTOR_NET_DEVICE_H
#define POINT_TO_POINT_TDTOR_NET_DEVICE_H

#include <cstring>
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/ptr.h"
#include "ns3/mac48-address.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/tdtcp-socket-base.h"

class PointToPointTdToRNetDevice : public PointToPointNetDevice 
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);
  
  /**
   * Construct a PointToPointTdToRNetDevice
   */
  PointToPointTdToRNetDevice ();

  /**
   * Destroy a PointToPointTdToRNetDevice
   *
   * This is the destructor for the PointToPointTdToRNetDevice.
   */
  virtual ~PointToPointTdToRNetDevice ();

  /**
   * As a hack, register the TdTcpSockets that this network device to notify 
   * network change for them. 
   */
  void RegisterTdTcpSocket (Ptr<TdTcpSocketBase> socket);

  /**
   * Schedule a network condition change at some time
   */
  void ScheduleNetchange (DataRate rate, Time delay, Time attime, uint8_t index);

private:
  
  /**
   * Compute when should the ToR tell the sockets about network change,
   * based on the current switch queue length
   */
  Time ComputeCurrQueueTxTime ();

  Time m_fullQueueTxTime;
  std::vector<Ptr<TdTcpSocketBase>> sockets;


}

#endif // POINT_TO_POINT_TDTOR_NET_DEVICE_H
