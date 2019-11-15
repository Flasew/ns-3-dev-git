#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/csma-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
//#include "ns3/multichannel-probe-module.h"

#include <unordered_map>
#include <ctime>
#include <iomanip>
#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TCP_LARGE_TRANSFER");

struct LinkParam {
  uint64_t bandwidth;
  std::string delay_s;
  uint64_t period;
  Time delay;
  LinkParam(uint64_t b, std::string d, uint64_t p): bandwidth(b), delay_s(d), period(p), delay(d){}
};

class TopoHelper {

public:
  static NodeContainer allNodes;
  static std::unordered_map<uint64_t, NetDeviceContainer> netList;
  static std::unordered_map<uint64_t, Ipv4InterfaceContainer> ifList;

  
  static std::normal_distribution<double> rjitnd;

  static void 
  Init(uint32_t n) {
    allNodes.Create(n);
  }

  static uint64_t 
  BuildIndex(uint32_t i, uint32_t j) {
    if (i > j) {
      uint32_t tmp = i;
      i = j;
      j = tmp;
    }
    uint64_t result = i;
    result <<= 32;
    result |= j;
    return result;
  }

  static NetDeviceContainer & 
  GetLink(uint32_t i, uint32_t j) {
    return netList[TopoHelper::BuildIndex(i, j)];
  }

  static Ipv4InterfaceContainer & 
  GetIf(uint32_t i, uint32_t j) {
    return ifList[TopoHelper::BuildIndex(i, j)];
  }

  static NodeContainer 
  GetContainerOf(std::initializer_list<uint32_t> args) {

    NodeContainer r;
    for (auto i: args) {
      r.Add(allNodes.Get(i));
    }
    return r;
  }

  static NodeContainer 
  Connect(PointToPointHelper & p2p, uint32_t i, uint32_t j) {

    NodeContainer n = TopoHelper::GetContainerOf({i, j});
    netList[TopoHelper::BuildIndex(i, j)] = p2p.Install(n);

    return n; 
  }

  static void 
  Connect(PointToPointHelper & p2p, 
    NodeContainer & nodes,
    uint32_t i, 
    uint32_t j) { 

    netList[TopoHelper::BuildIndex(i, j)] = p2p.Install(nodes);

  }

  static void
  AssignIP(Ipv4AddressHelper & iphelper, uint32_t i, uint32_t j) {

    ifList[TopoHelper::BuildIndex(i, j)] = 
    iphelper.Assign(netList[TopoHelper::BuildIndex(i, j)]);

  }

  static void 
  ChangeBW(uint64_t rate, Time delay, uint32_t i, uint32_t j, std::default_random_engine & gen, bool ratejitter=true) {

    if (ratejitter) {
      // int jit = rjitnd(gen);
      // if (-(jit*2) > 0 && (uint64_t)(-(jit*2)) >= rate)
      //   jit = 0;
      rate = rate + rjitnd(gen);
    }

    NS_LOG_INFO("Rate changing to " << rate);

    auto d = TopoHelper::GetLink(i, j);
    auto dev_p = d.Get(0);
    auto dev = DynamicCast<PointToPointNetDevice>(dev_p);
    dev->SetDataRate(DataRate(rate));
    //dev->SetDelay(delay);
    (DynamicCast<PointToPointChannel>(dev->GetChannel()))->SetDelay(delay);
    dev_p = d.Get(1);
    dev = DynamicCast<PointToPointNetDevice>(dev_p);
    dev->SetDataRate(DataRate(rate));
    //dev->SetDelay(delay);
    //(DynamicCast<PointToPointChannel>(dev->GetChannel()))->SetDelay(delay);

  }

  static void
  LinkUp(uint32_t i, uint32_t j) {

    auto if0 = ifList[TopoHelper::BuildIndex(i, j)].Get(0);
    if0.first->GetObject<Ipv4L3Protocol>()->GetInterface(if0.second)->SetUp();
    auto if1 = ifList[TopoHelper::BuildIndex(i, j)].Get(1);
    if1.first->GetObject<Ipv4L3Protocol>()->GetInterface(if1.second)->SetUp();

  }

  static void
  LinkDown(uint32_t i, uint32_t j) {

    auto if0 = ifList[TopoHelper::BuildIndex(i, j)].Get(0);
    if0.first->GetObject<Ipv4L3Protocol>()->GetInterface(if0.second)->SetDown();
    auto if1 = ifList[TopoHelper::BuildIndex(i, j)].Get(1);
    if1.first->GetObject<Ipv4L3Protocol>()->GetInterface(if1.second)->SetDown();

  }

};

NodeContainer TopoHelper::allNodes = NodeContainer();
std::unordered_map<uint64_t, NetDeviceContainer> TopoHelper::netList = std::unordered_map<uint64_t, NetDeviceContainer>();
std::unordered_map<uint64_t, Ipv4InterfaceContainer> TopoHelper::ifList = std::unordered_map<uint64_t, Ipv4InterfaceContainer>();


std::normal_distribution<double> TopoHelper::rjitnd = std::normal_distribution<double>();

std::normal_distribution<double> tjitnd;

std::ofstream qllog;
std::ofstream frlog;
std::ofstream lolog;
std::ofstream cwndlog;
std::ofstream config;

std::string bwt_str;
std::string hdelay = "10us";
std::string l_inter = "100us";
std::vector<LinkParam> bwt;

uint16_t sendPortBase = 50000;

double nsd = 5000.0;
double rjitter = 500000.0;
double tjitter = 4.0;
uint32_t dupackth = 3;
uint64_t max_bytes = 20000000000;
uint64_t simtime = 60;
uint64_t h_rate = 40000000000;
uint64_t rwnd = 262144;
uint64_t nflows = 1;
uint64_t q_size = 60;
bool nochange = false;
bool indivlog = true;
bool bidir = false;
bool rjitter_enable = true;
bool tjitter_enable = true;

std::default_random_engine  gen;

std::vector<Ptr<TdTcpSocketBase>> sockets;

int curr_rate = 0;

void CycleRate() {

  curr_rate = (curr_rate + 1) % bwt.size();
  TopoHelper::ChangeBW(bwt[curr_rate].bandwidth, bwt[curr_rate].delay, 2, 3, gen, rjitter_enable);

  uint64_t t = bwt[curr_rate].period;

  if (tjitter_enable) {
    int jit = tjitnd(gen);
    if ((jit) < 0 && (uint64_t)(-(jit*2)) >= t)
      jit = 0;
    t = t + jit;
  }

  for (auto & s: sockets) {
    s->ChangeActivateSubflow((uint8_t)curr_rate);
  }

  Simulator::Schedule(MicroSeconds(t), CycleRate);
}

void QlTrace (std::string ctxt, uint32_t oldValue, uint32_t newValue)
{

  if (qllog.is_open()) {
    if (ctxt[10] == '2')
      qllog << Simulator::Now().GetNanoSeconds() << ", 2, " << newValue << std::endl;
    else if (ctxt[10] == '3')
      qllog << Simulator::Now().GetNanoSeconds() << ", 3, " << newValue << std::endl;
  }

}

static void CwndTrace (std::string ctxt, uint32_t oldValue, uint32_t newValue)
{
  if (cwndlog.is_open()) 
    cwndlog <<  Simulator::Now().GetNanoSeconds() << ", " << ctxt << ", " << newValue << std::endl;
}

std::vector<uint64_t> frdata;
std::vector<uint64_t> lodata;

static void
CongStateTrace (std::string ctxt, 
  const TcpSocketState::TcpCongState_t oldValue, 
  const TcpSocketState::TcpCongState_t newValue)
{
  uint64_t flowid = stoi(ctxt);

  if (oldValue != TcpSocketState::CA_RECOVERY && newValue == TcpSocketState::CA_RECOVERY) {
    ++frdata[flowid]; 
    if (frlog.is_open())
      frlog <<  Simulator::Now().GetNanoSeconds() << ", " << ctxt << ", " << frdata[flowid] << std::endl;
  }

  if (oldValue != TcpSocketState::CA_LOSS && newValue == TcpSocketState::CA_LOSS) {
    ++lodata[flowid];
    if (lolog.is_open())
      lolog <<  Simulator::Now().GetNanoSeconds() << ", " << ctxt << ", " << lodata[flowid] << std::endl;
  }

}

static void
TcpStateTrace (std::string ctxt, 
  const TcpSocket::TcpStates_t oldValue, 
  const TcpSocket::TcpStates_t newValue)
{
  return;
}

std::vector<uint64_t> syndata;

static void TxPktTrace (std::string ctxt,  
  const Ptr< const Packet > packet, 
  const TcpHeader &header, 
  const Ptr< const TcpSocketBase > socket)
{
  if (header.GetFlags() & TcpHeader::SYN)
    ++syndata[stoi(ctxt)];
}

void ParseBWP(std::string & p) 
{
  std::istringstream ss(p);
  int c = 0;
  std::string d;
  uint64_t r = 0, pe = 0;

  while (ss) {
    std::string next;
    if (!getline(ss, next, ',')) break;

    if (c == 0) {
      r = std::stoull(next);
    }
    else if (c == 1) {
      d = next;
    }
    else if (c == 2) {
      pe = std::stoull(next);
      bwt.emplace_back(r, d, pe);
      c = 0;
        continue;
      }

      c++;
    }
  }

  int main(int argc, char * argv[]) {

    // Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
    // LogComponentEnable("TcpCongestionOps", LOG_LEVEL_INFO);
    LogComponentEnable("TdTcpSocketBase", LOG_ALL);
    // LogComponentEnable("TcpSocketBase", LOG_ALL);
    LogComponentEnable("TdTcpTxSubflow", LOG_ALL);
    LogComponentEnable("TdTcpRxSubflow", LOG_ALL);
    // LogComponentEnable("TdTcpMapping", LOG_ALL);
    // LogComponentEnable("TcpOptionTdTcp", LOG_ALL);
    // LogComponentEnable("TcpOption", LOG_ALL);
    // LogComponentEnable("TcpHeader", LOG_ALL);
    // LogComponentEnable("TcpTxBuffer", LOG_ALL);
    // LogComponentEnable("TCP_LARGE_TRANSFER", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_ALL);
    LogComponentEnable("BulkSendApplication", LOG_ALL);
    LogComponentEnable("TcpCongestionOps", LOG_ALL);
    LogComponentEnable("TcpBic", LOG_ALL);

    CommandLine cmd;
    cmd.AddValue("BWP", "Bandwidth pattern", bwt_str);
    cmd.AddValue("QueueLength", "Queue length of router", q_size);
    cmd.AddValue("HostRate", "Link rate between host and TOR", h_rate);
    cmd.AddValue("HostPropDelay", "Host propagation delay", hdelay);
    cmd.AddValue("MaxBytes", "Number of tests to test", max_bytes);
    cmd.AddValue("LogInterval", "interval between mcp probe", l_inter);
    cmd.AddValue("DupAckTh", "Duplicate ACK threashold for retransmit", dupackth);
    cmd.AddValue("Static", "If the topology should be static. Will use the first linkrate in specified pattern", nochange);
    cmd.AddValue("SimTime", "Max simulation time", simtime);
    cmd.AddValue("RWND", "Receiver windown size", rwnd);
    cmd.AddValue("NFlows", "Number of flows", nflows);
    cmd.AddValue("Nsd", "standard deviation for flow start time", nsd);
    cmd.AddValue("Rjitter", "BW rate Jitter.", rjitter);
    cmd.AddValue("Tjitter", "BW frequency Jitter.", tjitter);
    cmd.AddValue("Bidir", "Use bidirectional flow.", bidir);
    cmd.Parse(argc, argv);

    ParseBWP(bwt_str);

    std::random_device rd;
    gen = std::default_random_engine(rd());

    if (((int)rjitter) <= 0)
      rjitter_enable = false;
    else
      TopoHelper::rjitnd = std::normal_distribution<double>(0, rjitter);

    if (((int)tjitter) <= 0)
      tjitter_enable = false;
    else
      tjitnd = std::normal_distribution<double>(0, tjitter);

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;

    oss.str("");
    oss << "./qllog" << std::put_time(&tm, "_%m_%d_%Y_%H_%M_%S");
    qllog.open(oss.str());

    // oss.str("");
    // oss << "./frlog" << std::put_time(&tm, "_%m_%d_%Y_%H_%M_%S");
    // frlog.open(oss.str());

    // oss.str("");
    // oss << "./lolog" << std::put_time(&tm, "_%m_%d_%Y_%H_%M_%S");
    // lolog.open(oss.str());

    /*
    oss.str("");
    oss << "./cwndlog" << std::put_time(&tm, "_%m_%d_%Y_%H_%M_%S");
    cwndlog.open(oss.str());
  */

  oss.str("");
  oss << "./config" << std::put_time(&tm, "_%m_%d_%Y_%H_%M_%S.json");
  config.open(oss.str());

  config << "{" << std::endl;
  config << "\t\"timestamp\":\t" << std::put_time(&tm, "\"%m_%d_%Y_%H_%M_%S\",") << std::endl;
  config << "\t\"host_rate\":\t"<< h_rate << "," << std::endl;
  config << "\t\"host_propdelay\":\t\"" << hdelay << "\"," << std::endl;
  config << "\t\"max_bytes\":\t" << max_bytes << "," << std::endl;
  config << "\t\"queue_length\":\t" << q_size << "," << std::endl;
  config << "\t\"rwnd\":\t" << rwnd << "," << std::endl;
  config << "\t\"nflows\":\t" << nflows << "," << std::endl;
  config << "\t\"dupack\":\t" << dupackth << "," << std::endl;
  config << "\t\"bidirectional\":\t" << bidir << "," << std::endl;
  config << "\t\"rate_jitter\":\t" << (rjitter_enable ? rjitter : 0) << "," << std::endl;
  config << "\t\"dt_jitter\":\t" << (tjitter_enable ? tjitter : 0 )<< "," << std::endl;
  config << "\t\"static\":\t" << nochange << "," << std::endl;

  config << "\t\"bwp\":\t[" << std::endl;  

  for (uint64_t k = 0; k < bwt.size(); k++) {
    auto & item = bwt[k];
    config << "\t\t{\"rate\": " << item.bandwidth << ", \"delay\": \"" << item.delay_s << "\", \"period\": " << item.period << "}" << (k==bwt.size()-1 ? "" : ",") << std::endl;  
  }
  
  config << "\t]," << std::endl;  

  Time::SetResolution(Time::NS);

  Config::SetDefault ("ns3::TcpSocketState::MaxPacingRate", StringValue ("10Gbps"));
  Config::SetDefault ("ns3::TcpSocketState::EnablePacing", BooleanValue (true));
  // Config::SetDefault ("ns3::TcpSocketState::EnablePacing", BooleanValue (true));

  sockets = std::vector<Ptr<TdTcpSocketBase>>();
  //Packet::EnablePrinting();

  TopoHelper::Init(4);

  PointToPointHelper hp2p;
  PointToPointHelper sp2p;
  hp2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(h_rate)));
  hp2p.SetDeviceAttribute("Mtu", UintegerValue(1514));
  hp2p.SetChannelAttribute("Delay", StringValue(hdelay));
  // hp2p.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
  TopoHelper::Connect(hp2p, 0, 2);
  TopoHelper::Connect(hp2p, 1, 3);

  sp2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(bwt[0].bandwidth)));
  sp2p.SetDeviceAttribute("Mtu", UintegerValue(1514));
  sp2p.SetChannelAttribute("Delay", StringValue(bwt[0].delay_s));
  sp2p.SetChannelType("PointToPointOrderedChannel");
  sp2p.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

  TopoHelper::Connect(sp2p, 2, 3);

  InternetStackHelper stack;
  stack.Install(TopoHelper::allNodes);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc("ns3::FifoQueueDisc");
  tch.AddInternalQueues(handle, 1, "ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize(BYTES, (q_size*1514))));
  tch.Install(TopoHelper::GetLink(2, 3).Get(0));
  tch.Install(TopoHelper::GetLink(2, 3).Get(1));

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  TopoHelper::AssignIP(address, 0, 2);
  address.SetBase("10.0.1.0", "255.255.255.0");
  TopoHelper::AssignIP(address, 1, 3);
  address.SetBase("10.1.0.0", "255.255.255.0");
  TopoHelper::AssignIP(address, 2, 3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  frdata = std::vector<uint64_t>(nflows*2, 0);
  lodata = std::vector<uint64_t>(nflows*2, 0);
  syndata = std::vector<uint64_t>(nflows*2, 0);

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1424));
  Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (false));
  Config::SetDefault ("ns3::TcpSocketBase::Timestamp", BooleanValue (false));
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (rwnd));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (rwnd));

  std::normal_distribution<double> nd(0.0, nsd);
  double startdiff;

  uint16_t portInit = 2048;  

  Address sinkAddressr(InetSocketAddress(TopoHelper::GetIf(1, 3).GetAddress(0), portInit));
  std::vector< Ptr<BulkSendApplication> > sendAppsr;

  Address sinkAddressl(InetSocketAddress(TopoHelper::GetIf(0, 2).GetAddress(0), portInit));
  std::vector< Ptr<BulkSendApplication> > sendAppsl;

  uint64_t i = 0;
  for (uint64_t j = 0; j < nflows; j++) {
    i++;

    //Ptr<TdTcpSocketBase> ns3TcpSocket = CreateObject<TdTcpSocketBase>(TopoHelper::allNodes.Get(1));
    //Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (TopoHelper::allNodes.Get(0), TcpSocketFactory::GetTypeId());
    //Ptr<TcpSocketBase> sb = DynamicCast<TcpSocketBase>(ns3TcpSocket);
    Ptr<TcpL4Protocol> tcp = TopoHelper::allNodes.Get(0)->GetObject<TcpL4Protocol>();
    Ptr<Socket> ns3TcpSocket = tcp->CreateSocket (TcpNewReno::GetTypeId(), TdTcpSocketBase::GetTypeId(), true);
    Ptr<TdTcpSocketBase> sb = DynamicCast<TdTcpSocketBase>(ns3TcpSocket);
    // Ptr<TcpRecoveryOps> rec = CreateObject<TcpPrrRecovery>();
    // sb->SetRecoveryAlgorithm(rec);
    sb->SetRetxThresh(dupackth);
    // sb->SetMaxPacingRate(0, DataRate("10Gbps"));
    // sb->SetMaxPacingRate(1, DataRate("1Gbps"));
    // sb->SetPacingRatio(0, 10);
    // sb->SetPacingRatio(1, 1);

    sockets.push_back(sb);

    std::cout << ns3TcpSocket->Bind(InetSocketAddress(sendPortBase+i)) << std::endl;

    ns3TcpSocket->TraceConnect("CongestionWindow", std::to_string(i),  MakeCallback (&CwndTrace));
    ns3TcpSocket->TraceConnect("CongState", std::to_string(i),  MakeCallback (&CongStateTrace));
    ns3TcpSocket->TraceConnect("State", std::to_string(i),  MakeCallback (&TcpStateTrace));
    ns3TcpSocket->TraceConnect("Tx", std::to_string(i),  MakeCallback (&TxPktTrace));

    Ptr<BulkSendApplication> app = CreateObject<BulkSendApplication> ();
    app->SetUp(ns3TcpSocket, sinkAddressr, max_bytes/nflows);
    TopoHelper::allNodes.Get(0)->AddApplication(app);

    if ((startdiff = nd(gen)) < 0.1)
      startdiff = -startdiff;

    app->SetStartTime (MicroSeconds (1000000.0 + startdiff));

    sendAppsl.push_back(app);
  }

  if (bidir) {

    for (uint64_t j = 0; j < nflows; j++) {
      i++;

      Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (TopoHelper::allNodes.Get(1), TcpSocketFactory::GetTypeId());
      //Ptr<TcpRecoveryOps> rec = CreateObject<TcpPrrRecovery>();

      Ptr<TcpSocketBase> sb = DynamicCast<TcpSocketBase>(ns3TcpSocket);
      //sb->SetRecoveryAlgorithm(rec);
      sb->SetRetxThresh(dupackth);

      ns3TcpSocket->Bind(InetSocketAddress(sendPortBase+i));

      ns3TcpSocket->TraceConnect ("CongestionWindow", std::to_string(i),  MakeCallback (&CwndTrace));
      ns3TcpSocket->TraceConnect ("CongState", std::to_string(i),  MakeCallback (&CongStateTrace));
      ns3TcpSocket->TraceConnect ("State", std::to_string(i),  MakeCallback (&TcpStateTrace));
      ns3TcpSocket->TraceConnect ("Tx", std::to_string(i),  MakeCallback (&TxPktTrace));

      Ptr<BulkSendApplication> app = CreateObject<BulkSendApplication> ();
      app->SetUp(ns3TcpSocket, sinkAddressl, max_bytes/nflows);
      TopoHelper::allNodes.Get(1)->AddApplication(app);

      if ((startdiff = nd(gen)) < 1)
        startdiff = -startdiff;

      app->SetStartTime (MicroSeconds (1000000.0 + startdiff));
      sendAppsr.push_back(app);

    }
  }

  PacketSinkHelper sink("ns3::TcpSocketFactory",
    InetSocketAddress (Ipv4Address::GetAny(), portInit));
  // ApplicationContainer sinkAppr = sink.Install(TopoHelper::allNodes.Get(1));
  // sinkAppr.Start (Seconds (0.0));
  Ptr<TcpL4Protocol> tcp = TopoHelper::allNodes.Get(1)->GetObject<TcpL4Protocol>();
  Ptr<Socket> ns3TcpSocket = tcp->CreateSocket (TcpNewReno::GetTypeId(), TdTcpSocketBase::GetTypeId(), true);
  Ptr<TdTcpSocketBase> sb = DynamicCast<TdTcpSocketBase>(ns3TcpSocket);
  // Ptr<TcpRecoveryOps> rec = CreateObject<TcpPrrRecovery>();
  // sb->SetRecoveryAlgorithm(rec);
  // sb->SetMaxPacingRate(0, DataRate("10Gbps"));
  // sb->SetMaxPacingRate(1, DataRate("1Gbps"));
  // sb->SetPacingRatio(0, 10);
  // sb->SetPacingRatio(1, 1);
  Ptr<PacketSink> app = CreateObject<PacketSink> ();
  std::cout << "sink bind: " << ns3TcpSocket->Bind(InetSocketAddress(portInit)) << std::endl;
  app->SetUp(ns3TcpSocket, Ipv4Address::GetAny(), true);

  sockets.push_back(sb);

  TopoHelper::allNodes.Get(1)->AddApplication(app);

  app->SetStartTime (MicroSeconds (0));

  if (bidir) {
    ApplicationContainer sinkAppl = sink.Install(TopoHelper::allNodes.Get(0));
    sinkAppl.Start (Seconds (0.0));
  }

  Config::Connect(
    "/NodeList/2/$ns3::TrafficControlLayer/RootQueueDiscList/1/BytesInQueue",
    MakeCallback (&QlTrace));
  Config::Connect(
    "/NodeList/3/$ns3::TrafficControlLayer/RootQueueDiscList/1/BytesInQueue",
    MakeCallback (&QlTrace));

  if (!nochange)
    Simulator::Schedule(MicroSeconds(1000000+bwt[0].period), CycleRate);

  Simulator::Stop(Seconds(simtime));
  Simulator::Run();

  oss.str("");
  oss << "mon" << std::put_time(&tm, "_%m_%d_%Y_%H_%M_%S") << ".xml";
  flowMonitor->SerializeToXmlFile(oss.str(), true, true);

  Simulator::Destroy();

  config << "\t\"flowdata\":\t[" << std::endl;

  if (bidir)
    nflows *= 2;

  for (uint64_t k = 0; k < nflows; k++) {
    config << "\t\t{" << std::endl; 
    config << "\t\t\t\"port\":\t" << (1 + k + sendPortBase) << "," << std::endl; 
    config << "\t\t\t\"retransmit\":\t" << frdata[k] << "," << std::endl; 
    config << "\t\t\t\"timeout\":\t" << lodata[k] << "," << std::endl; 
    config << "\t\t\t\"syn_sent\":\t" << syndata[k] << std::endl; 
    config << "\t\t}" << (k == nflows-1 ? "" : ",") << std::endl; 
  }
  config << "\t]" << std::endl;

  config << "}" << std::endl;  
  
  qllog.close();
  // frlog.close();
  // lolog.close();
  // cwndlog.close();

  config.close();

  return 0;

}
