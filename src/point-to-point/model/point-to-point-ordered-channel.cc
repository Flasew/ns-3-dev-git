/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
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

#include <iostream>

#include "point-to-point-ordered-channel.h"
#include "point-to-point-net-device.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PointToPointOrderedChannel");

NS_OBJECT_ENSURE_REGISTERED (PointToPointOrderedChannel);

TypeId
PointToPointOrderedChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PointToPointOrderedChannel")
    .SetParent<PointToPointChannel> ()
    .SetGroupName ("PointToPoint")
    .AddConstructor<PointToPointOrderedChannel> ()
  ;
  return tid;
}

PointToPointOrderedChannel::PointToPointOrderedChannel ()
  : PointToPointChannel ()
{
  m_packetQueues = new std::list<Ptr<Packet>>[2];
}

PointToPointOrderedChannel::~PointToPointOrderedChannel ()
{
  delete[] m_packetQueues;
}

bool
PointToPointOrderedChannel::TransmitStart (
  Ptr<const Packet> p,
  Ptr<PointToPointNetDevice> src,
  Time txTime)
{
  NS_LOG_FUNCTION (this << p << src);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  NS_ASSERT (m_link[0].m_state != INITIALIZING);
  NS_ASSERT (m_link[1].m_state != INITIALIZING);

  uint32_t wire = src == m_link[0].m_src ? 0 : 1;
  m_packetQueues[wire].push_back(p->Copy());
  Time totDelay = m_delay + txTime;
  Simulator::Schedule (totDelay, &PointToPointOrderedChannel::PacketGo, this, wire);

  return true;
}

void
PointToPointOrderedChannel::PacketGo(uint32_t wire)
{
  NS_LOG_FUNCTION (this << wire);

  NS_ASSERT (m_packetQueues[wire].size() != 0);

  Ptr<Packet> p = m_packetQueues[wire].front();
  m_packetQueues[wire].pop_front();

  //std::cout << wire << ": " << p->ToString() << std::endl;

  Simulator::ScheduleWithContext (m_link[wire].m_dst->GetNode ()->GetId (),
                                  Time(Seconds(0)), &PointToPointNetDevice::Receive,
                                  m_link[wire].m_dst, p->Copy ());
  //m_link[wire].m_dst->Receive(p->Copy());

  // Call the tx anim callback on the net device
  //m_txrxPointToPoint (p, src, m_link[wire].m_dst, txTime, totDelay);
}

} // namespace ns3
