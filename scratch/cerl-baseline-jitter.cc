#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-linux-reno.h"

#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <numeric>
#include <string>

#include "tcp-cerl.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("CerlBaseline");


static std::ofstream g_cwndLog;

static void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
{
    g_cwndLog << std::fixed << std::setprecision(4)
              << Simulator::Now().GetSeconds() << ","
              << oldCwnd << ","
              << newCwnd << "\n";
}

static void ConnectCwndTrace(const std::string& logPath)
{
    g_cwndLog.open(logPath);
    g_cwndLog << "time_s,cwnd_old_bytes,cwnd_new_bytes\n";
    Config::ConnectWithoutContext(
        "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
        MakeCallback(&CwndTracer));
}

class ThroughputSampler
{
public:
    void Init(const std::string& path, double interval)
    {
        m_interval = interval;
        m_out.open(path);
        m_out << "time_s,throughput_mbps\n";
    }

    void Start(Ptr<PacketSink> sink, double firstAt)
    {
        m_sink      = sink;
        m_lastBytes = 0;
        Simulator::Schedule(Seconds(firstAt), &ThroughputSampler::Sample, this);
    }

private:
    void Sample()
    {
        uint64_t rx   = m_sink->GetTotalRx();
        double   mbps = double(rx - m_lastBytes) * 8.0 / (m_interval * 1e6);
        m_lastBytes   = rx;
        m_out << std::fixed << std::setprecision(3)
              << Simulator::Now().GetSeconds() << "," << mbps << "\n";
        Simulator::Schedule(Seconds(m_interval), &ThroughputSampler::Sample, this);
    }

    Ptr<PacketSink> m_sink;
    std::ofstream   m_out;
    double          m_interval {0.5};
    uint64_t        m_lastBytes{0};
};

void setup(int argc, char* argv[],
           std::string& protocol, double& errorRate,
           double& duration, double& jitterMbps, std::string& outDir, std::string& dir)
{
    TcpCerl::GetTypeId();
    TcpCerlA::GetTypeId();
    TcpCerlM::GetTypeId();

    CommandLine cmd(__FILE__);
    cmd.AddValue("protocol",  "cerl | acerl | mcerl | newreno", protocol);
    cmd.AddValue("errorRate", "random loss probability [0,1]",  errorRate);
    cmd.AddValue("duration",  "simulation duration (s)",        duration);
    cmd.AddValue("jitterMbps","Bursty UDP background traffic",  jitterMbps);
    cmd.AddValue("outDir",    "output directory",               outDir);
    cmd.Parse(argc, argv);

    dir = outDir + "/" + protocol;
    std::filesystem::create_directories(dir);

    if (protocol == "cerl")
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TcpCerl::GetTypeId()));
    else if (protocol == "acerl")
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TcpCerlA::GetTypeId()));
    else if (protocol == "mcerl")
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TcpCerlM::GetTypeId()));
    else
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TcpLinuxReno::GetTypeId()));

    Config::SetDefault("ns3::TcpSocket::SndBufSize",  UintegerValue(1u << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize",  UintegerValue(1u << 20));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1460));
}

int main(int argc, char* argv[])
{
    std::string protocol   {"cerl"};
    double      errorRate  {0.01};
    double      duration   {30.0};
    double      jitterMbps {0.0}; // Default to no jitter
    std::string outDir     {"cerl-results"};
    std::string dir;
    setup(argc, argv, protocol, errorRate, duration, jitterMbps, outDir, dir);

    Ptr<Node> sender   = CreateObject<Node>();
    Ptr<Node> routerL  = CreateObject<Node>();
    Ptr<Node> routerR  = CreateObject<Node>();
    Ptr<Node> receiver = CreateObject<Node>();

    InternetStackHelper inet;
    inet.Install(sender);
    inet.Install(routerL);
    inet.Install(routerR);
    inet.Install(receiver);

    Ipv4AddressHelper addr;
    
    // Sender --- RouterL  (100 Mbps, 1 ms, no loss)
    PointToPointHelper access;
    access.SetDeviceAttribute ("DataRate", StringValue("100Mbps"));
    access.SetChannelAttribute("Delay",    StringValue("1ms"));
    addr.SetBase("10.1.1.0", "255.255.255.0");
    addr.Assign(access.Install(sender, routerL));

    // RouterL === RouterR  (5 Mbps bottleneck, 20 ms, random loss)
    PointToPointHelper bottle;
    bottle.SetDeviceAttribute ("DataRate", StringValue("5Mbps"));
    bottle.SetChannelAttribute("Delay",    StringValue("20ms"));
    bottle.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("50p"));
    NetDeviceContainer devLR = bottle.Install(routerL, routerR);
    addr.SetBase("10.2.1.0", "255.255.255.0");
    addr.Assign(devLR);

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(errorRate));
    em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    devLR.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    // RouterR --- Receiver  (100 Mbps, 1 ms, no loss)
    NetDeviceContainer     devRR   = access.Install(routerR, receiver);
    addr.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceRR = addr.Assign(devRR);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Application Setup
    constexpr uint16_t PORT     = 9;
    Ipv4Address        recvAddr = ifaceRR.GetAddress(1);

    PacketSinkHelper sinkH("ns3::TcpSocketFactory",
                           InetSocketAddress(Ipv4Address::GetAny(), PORT));
    ApplicationContainer sinkApp = sinkH.Install(receiver);
    sinkApp.Start(Seconds(0.5));
    sinkApp.Stop (Seconds(duration));
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));

    BulkSendHelper bulkH("ns3::TcpSocketFactory",
                         InetSocketAddress(recvAddr, PORT));
    bulkH.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sendApp = bulkH.Install(sender);
    sendApp.Start(Seconds(1.0));
    sendApp.Stop (Seconds(duration - 0.5));

    // Application Setup: Jitter-Inducing UDP Flow
    if (jitterMbps > 0.0) {
        uint16_t udpPort = 5000;
        
        PacketSinkHelper udpSink("ns3::UdpSocketFactory", 
                                 InetSocketAddress(Ipv4Address::GetAny(), udpPort));
        ApplicationContainer udpSinkApp = udpSink.Install(receiver);
        udpSinkApp.Start(Seconds(0.1));
        udpSinkApp.Stop(Seconds(duration));

        OnOffHelper udpOnOff("ns3::UdpSocketFactory", 
                             InetSocketAddress(recvAddr, udpPort));
        
        udpOnOff.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.05]"));
        udpOnOff.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.05]"));
        udpOnOff.SetAttribute("DataRate", StringValue(std::to_string(jitterMbps) + "Mbps"));
        udpOnOff.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer udpApp = udpOnOff.Install(sender);
        udpApp.Start(Seconds(0.5));
        udpApp.Stop(Seconds(duration - 0.5));
        
        std::cout << "[INFO] Injected " << jitterMbps << " Mbps of bursty UDP traffic to induce jitter.\n";
    }

    Simulator::Schedule(Seconds(1.1), &ConnectCwndTrace, dir + "/cwnd.csv");

    ThroughputSampler sampler;
    sampler.Init(dir + "/throughput.csv", 0.5);
    sampler.Start(sink, 1.5);

    FlowMonitorHelper  fmH;
    Ptr<FlowMonitor>   monitor = fmH.InstallAll();

    std::cout << "\n[CERL] protocol=" << protocol
              << "  errorRate=" << errorRate * 100.0 << "%\n";

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    monitor->CheckForLostPackets();

    std::ofstream csv(dir + "/flow_stats.csv");
    csv << "protocol,tx_packets,rx_packets,lost_packets,throughput_mbps,delay_ms\n";

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmH.GetClassifier());

    for (auto& [id, s] : monitor->GetFlowStats())
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(id);
        
        if (t.destinationPort != PORT) continue;

        double dur   = (s.timeLastRxPacket - s.timeFirstTxPacket).GetSeconds();
        double tput  = (dur > 0) ? (s.rxBytes * 8.0 / dur / 1e6) : 0.0;
        double delay = (s.rxPackets > 0)
                       ? s.delaySum.GetMilliSeconds() / s.rxPackets : 0.0;

        csv << protocol << "," << s.txPackets << "," << s.rxPackets << ","
            << s.lostPackets << "," << std::fixed << std::setprecision(3)
            << tput << "," << delay << "\n";

        std::cout << "  Throughput : " << tput  << " Mbps\n"
                  << "  Lost pkts  : " << s.lostPackets << "\n"
                  << "  Mean delay : " << delay << " ms\n";
    }
    std::cout << "  Saved to   : " << dir << "/\n";

    Simulator::Destroy();
    return 0;
}