#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-linux-reno.h"
#include "ns3/wifi-module.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "tcp-cerl.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("CerlBaselineMobility");

void setup(int argc, char* argv[], std::string& protocol, double& velocity, 
           double& errorRate, double& duration, uint32_t& nNodes, 
           uint32_t& nFlows, uint32_t& pps, std::string& outDir)
{
    TcpCerl::GetTypeId();
    TcpCerlA::GetTypeId();
    TcpCerlM::GetTypeId();

    CommandLine cmd(__FILE__);
    cmd.AddValue("protocol",  "cerl | acerl | mcerl | newreno", protocol);
    cmd.AddValue("velocity",  "receiver move speed in m/s",     velocity);
    cmd.AddValue("errorRate", "random loss probability [0,1]",  errorRate);
    cmd.AddValue("duration",  "simulation duration (s)",        duration);
    cmd.AddValue("nNodes",    "total number of nodes",          nNodes);
    cmd.AddValue("nFlows",    "total number of flows",          nFlows);
    cmd.AddValue("pps",       "packets per second per flow",    pps);
    cmd.AddValue("outDir",    "output directory",               outDir);
    cmd.Parse(argc, argv);

    std::filesystem::create_directories(outDir);

    if (protocol == "cerl")
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCerl::GetTypeId()));
    else if (protocol == "acerl")
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCerlA::GetTypeId()));
    else if (protocol == "mcerl")
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCerlM::GetTypeId()));
    else
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpLinuxReno::GetTypeId()));

    Config::SetDefault("ns3::TcpSocket::SndBufSize",  UintegerValue(1u << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize",  UintegerValue(1u << 20));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1460));
}

static YansWifiChannelHelper MakeChannel()
{
    YansWifiChannelHelper ch = YansWifiChannelHelper::Default();
    ch.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    return ch;
}

int main(int argc, char* argv[])
{
    std::string protocol  {"cerl"};
    double      velocity  {10.0};   
    double      errorRate {0.05};  
    double      duration  {30.0};
    uint32_t    nNodes    {40};
    uint32_t    nFlows    {20};
    uint32_t    pps       {300};
    std::string outDir    {"mobility-results"};
    
    setup(argc, argv, protocol, velocity, errorRate, duration, nNodes, nFlows, pps, outDir);

    uint32_t halfNodes = nNodes / 2;
    if (halfNodes == 0) halfNodes = 1;

    NodeContainer senders, receivers;
    senders.Create(halfNodes);
    receivers.Create(halfNodes);
    
    Ptr<Node> routerL = CreateObject<Node>(); 
    Ptr<Node> routerR = CreateObject<Node>(); 

    InternetStackHelper inet;
    inet.Install(senders);
    inet.Install(receivers);
    inet.Install(routerL);
    inet.Install(routerR);

    // WiFi Setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
    WifiMacHelper mac;

    // (Senders to RouterL)
    YansWifiPhyHelper phyLeft;
    phyLeft.SetChannel(MakeChannel().Create());
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("cerl-left")));
    NetDeviceContainer apLeftDev = wifi.Install(phyLeft, mac, routerL);
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("cerl-left")), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staLeftDevs = wifi.Install(phyLeft, mac, senders);

    // (RouterR to Receivers)
    YansWifiPhyHelper phyRight;
    phyRight.SetChannel(MakeChannel().Create());
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("cerl-right")));
    NetDeviceContainer apRightDev = wifi.Install(phyRight, mac, routerR);
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("cerl-right")), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staRightDevs = wifi.Install(phyRight, mac, receivers);

    // Wired Bottleneck
    PointToPointHelper bottle;
    bottle.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottle.SetChannelAttribute("Delay", StringValue("20ms"));
    bottle.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("50p"));
    NetDeviceContainer devLR = bottle.Install(routerL, routerR);
    
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(errorRate));
    em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    devLR.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    // Energy Model for Mobile Receivers
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000.0));
    ns3::energy::EnergySourceContainer rcvEnergySources = basicSourceHelper.Install(receivers);
    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Install(staRightDevs, rcvEnergySources);

    // IP Addressing
    Ipv4AddressHelper addr;
    addr.SetBase("10.1.1.0", "255.255.255.0");
    addr.Assign(apLeftDev);
    addr.Assign(staLeftDevs);

    addr.SetBase("10.2.1.0", "255.255.255.0");
    addr.Assign(devLR);

    addr.SetBase("10.3.1.0", "255.255.255.0");
    addr.Assign(apRightDev);
    Ipv4InterfaceContainer ifaceRight = addr.Assign(staRightDevs);

    // Mobility
    MobilityHelper mobFixed, mobMobile;
    
    Ptr<ListPositionAllocator> posFixed = CreateObject<ListPositionAllocator>();
    posFixed->Add(Vector(10.0, 0.0, 0.0)); // routerL
    posFixed->Add(Vector(60.0, 0.0, 0.0)); // routerR
    for(uint32_t i = 0; i < halfNodes; ++i) posFixed->Add(Vector(0.0, i * 2.0, 0.0)); // Senders
    
    mobFixed.SetPositionAllocator(posFixed);
    mobFixed.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobFixed.Install(routerL);
    mobFixed.Install(routerR);
    mobFixed.Install(senders);

    Ptr<ListPositionAllocator> posMobile = CreateObject<ListPositionAllocator>();
    for(uint32_t i = 0; i < halfNodes; ++i) posMobile->Add(Vector(65.0, i * 2.0, 0.0)); // Receivers
    
    mobMobile.SetPositionAllocator(posMobile);
    mobMobile.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobMobile.Install(receivers);
    
    for(uint32_t i = 0; i < halfNodes; ++i) {
        receivers.Get(i)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(velocity, 0.0, 0.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications
    uint16_t port = 9;
    std::string dataRateStr = std::to_string(pps * 1460 * 8) + "bps";

    for (uint32_t i = 0; i < nFlows; ++i) {
        uint32_t srcNode = i % halfNodes;
        uint32_t dstNode = i % halfNodes;

        PacketSinkHelper sinkH("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        ApplicationContainer sinkApp = sinkH.Install(receivers.Get(dstNode));
        sinkApp.Start(Seconds(0.5));
        sinkApp.Stop(Seconds(duration));

        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(ifaceRight.GetAddress(dstNode), port + i));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue(dataRateStr));
        onoff.SetAttribute("PacketSize", UintegerValue(1460));

        ApplicationContainer sendApp = onoff.Install(senders.Get(srcNode));
        double startTime = 1.0 + (i * 0.05); 
        sendApp.Start(Seconds(startTime));
        sendApp.Stop(Seconds(duration - 0.5));
    }

    FlowMonitorHelper fmH;
    Ptr<FlowMonitor> monitor = fmH.InstallAll();

    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    monitor->CheckForLostPackets();

    // Metrics
    uint64_t totalTxPackets = 0, totalRxPackets = 0, totalLostPackets = 0, totalRxBytes = 0;
    double totalDelayMs = 0.0;
    uint32_t validFlows = 0;

    for (auto& [id, s] : monitor->GetFlowStats()) {
        totalTxPackets += s.txPackets;
        totalRxPackets += s.rxPackets;
        totalLostPackets += s.lostPackets;
        totalRxBytes += s.rxBytes;
        if (s.rxPackets > 0) {
            totalDelayMs += s.delaySum.GetMilliSeconds() / s.rxPackets;
            validFlows++;
        }
    }

    double totalEnergyConsumed = 0.0;
    for (auto iter = rcvEnergySources.Begin(); iter != rcvEnergySources.End(); ++iter) {
        totalEnergyConsumed += (10000.0 - (*iter)->GetRemainingEnergy());
    }

    double netThroughputMbps = (totalRxBytes * 8.0) / ((duration - 1.0) * 1e6);
    double avgDelayMs = (validFlows > 0) ? (totalDelayMs / validFlows) : 0.0;
    double pdr = (totalTxPackets > 0) ? ((double)totalRxPackets / totalTxPackets) : 0.0;
    double dropRatio = (totalTxPackets > 0) ? ((double)totalLostPackets / totalTxPackets) : 0.0;
    double avgEnergyJ = totalEnergyConsumed / halfNodes;

    // Output to CSV
    std::string csvFile = outDir + "/master_results_mobility.csv";
    bool writeHeader = !std::filesystem::exists(csvFile);
    std::ofstream csv(csvFile, std::ios_base::app);
    
    if (writeHeader) {
        csv << "Protocol,Nodes,Flows,PPS,Velocity,Throughput_Mbps,AvgDelay_ms,PDR,DropRatio,AvgEnergy_J\n";
    }
    
    csv << protocol << "," << nNodes << "," << nFlows << "," << pps << "," << velocity << ","
        << std::fixed << std::setprecision(4) << netThroughputMbps << "," 
        << avgDelayMs << "," << pdr << "," << dropRatio << "," << avgEnergyJ << "\n";

    Simulator::Destroy();
    return 0;
}