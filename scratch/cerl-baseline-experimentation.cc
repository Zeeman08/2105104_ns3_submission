#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-linux-reno.h"
#include "tcp-cerl.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("CerlBaselineWired");

void setup(int argc, char* argv[], std::string& protocol, double& errorRate,
           double& duration, uint32_t& nNodes, uint32_t& nFlows, uint32_t& pps, 
           std::string& outDir)
{
    TcpCerl::GetTypeId();
    TcpCerlA::GetTypeId();
    TcpCerlM::GetTypeId();

    CommandLine cmd(__FILE__);
    cmd.AddValue("protocol",  "cerl | acerl | mcerl | newreno", protocol);
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

int main(int argc, char* argv[])
{
    std::string protocol  {"cerl"};
    double      errorRate {0.05};
    double      duration  {30.0};
    uint32_t    nNodes    {40};
    uint32_t    nFlows    {20};
    uint32_t    pps       {300};
    std::string outDir    {"wired-results"};
    
    setup(argc, argv, protocol, errorRate, duration, nNodes, nFlows, pps, outDir);

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

    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    access.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper bottle;
    bottle.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottle.SetChannelAttribute("Delay", StringValue("20ms"));
    bottle.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("50p"));

    Ipv4AddressHelper addr;
    
    // Connect Senders to RouterL
    NetDeviceContainer senderDevs, routerLDevs;
    for (uint32_t i = 0; i < halfNodes; ++i) {
        NetDeviceContainer link = access.Install(senders.Get(i), routerL);
        senderDevs.Add(link.Get(0));
        routerLDevs.Add(link.Get(1));
        std::string subnet = "10.1." + std::to_string(i + 1) + ".0";
        addr.SetBase(subnet.c_str(), "255.255.255.0");
        addr.Assign(link);
    }

    // Bottleneck Link
    NetDeviceContainer bottleDevs = bottle.Install(routerL, routerR);
    addr.SetBase("10.2.1.0", "255.255.255.0");
    addr.Assign(bottleDevs);

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(errorRate));
    em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    bottleDevs.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    // Connect RouterR to Receivers
    NetDeviceContainer routerRDevs, receiverDevs;
    Ipv4InterfaceContainer receiverIfaces;
    for (uint32_t i = 0; i < halfNodes; ++i) {
        NetDeviceContainer link = access.Install(routerR, receivers.Get(i));
        routerRDevs.Add(link.Get(0));
        receiverDevs.Add(link.Get(1));
        std::string subnet = "10.3." + std::to_string(i + 1) + ".0";
        addr.SetBase(subnet.c_str(), "255.255.255.0");
        receiverIfaces.Add(addr.Assign(link).Get(1));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications setup
    uint16_t port = 9;
    std::string dataRateStr = std::to_string(pps * 1460 * 8) + "bps";

    for (uint32_t i = 0; i < nFlows; ++i) {
        uint32_t srcNode = i % halfNodes;
        uint32_t dstNode = i % halfNodes;

        // Sink
        PacketSinkHelper sinkH("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        ApplicationContainer sinkApp = sinkH.Install(receivers.Get(dstNode));
        sinkApp.Start(Seconds(0.5));
        sinkApp.Stop(Seconds(duration));

        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(receiverIfaces.GetAddress(dstNode), port + i));
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

    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalLostPackets = 0;
    uint64_t totalRxBytes = 0;
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

    double netThroughputMbps = (totalRxBytes * 8.0) / ((duration - 1.0) * 1e6);
    double avgDelayMs = (validFlows > 0) ? (totalDelayMs / validFlows) : 0.0;
    double pdr = (totalTxPackets > 0) ? ((double)totalRxPackets / totalTxPackets) : 0.0;
    double dropRatio = (totalTxPackets > 0) ? ((double)totalLostPackets / totalTxPackets) : 0.0;

    // Append to master CSV
    std::string csvFile = outDir + "/master_results.csv";
    bool writeHeader = !std::filesystem::exists(csvFile);
    std::ofstream csv(csvFile, std::ios_base::app);
    
    if (writeHeader) {
        csv << "Protocol,Nodes,Flows,PPS,Throughput_Mbps,AvgDelay_ms,PDR,DropRatio\n";
    }
    
    csv << protocol << "," << nNodes << "," << nFlows << "," << pps << "," 
        << std::fixed << std::setprecision(4) << netThroughputMbps << "," 
        << avgDelayMs << "," << pdr << "," << dropRatio << "\n";

    Simulator::Destroy();
    return 0;
}