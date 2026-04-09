#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-linux-reno.h"
#include "ns3/wifi-module.h"

#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <numeric>

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
    double          m_interval{0.5};
    uint64_t        m_lastBytes{0};
};

void setup(int argc, char* argv[],
           std::string& protocol, double& velocity, double& errorRate,
           double& duration, std::string& outDir, std::string& dir)
{
    TcpCerl::GetTypeId();
    TcpCerlA::GetTypeId();
    TcpCerlM::GetTypeId();

    CommandLine cmd(__FILE__);
    cmd.AddValue("protocol", "cerl | acerl | mcerl | newreno", protocol);
    cmd.AddValue("velocity", "receiver move speed in m/s",      velocity);
    cmd.AddValue("duration", "simulation duration (s)",         duration);
    cmd.AddValue("outDir",   "output directory",                outDir);
    cmd.AddValue("errorRate", "random loss probability [0,1]", errorRate);
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

static YansWifiChannelHelper MakeChannel()
{
    YansWifiChannelHelper ch = YansWifiChannelHelper::Default();
    ch.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    return ch;
}


int main(int argc, char* argv[])
{
    std::string protocol{"cerl"};
    double      velocity{5.0};   // m/s
    double      duration{30.0};
    double errorRate {0.05};  
    std::string outDir{"cerl-mobility-results"};
    std::string dir;
    setup(argc, argv, protocol, velocity, errorRate, duration, outDir, dir);

    
    Ptr<Node> sender   = CreateObject<Node>(); // STA on left
    Ptr<Node> routerL  = CreateObject<Node>(); // AP1  
    Ptr<Node> routerR  = CreateObject<Node>(); // AP2  
    Ptr<Node> receiver = CreateObject<Node>(); // STA on right (moves)

    InternetStackHelper inet;
    inet.Install(sender);
    inet.Install(routerL);
    inet.Install(routerR);
    inet.Install(receiver);

    
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    WifiMacHelper mac;

    // ---- Left: Sender(STA) <--> RouterL(AP) ---------------------------
    YansWifiPhyHelper phyLeft;
    phyLeft.SetChannel(MakeChannel().Create());

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("cerl-left")));
    NetDeviceContainer apLeftDev = wifi.Install(phyLeft, mac, routerL);

    mac.SetType("ns3::StaWifiMac",
                "Ssid",            SsidValue(Ssid("cerl-left")),
                "ActiveProbing",   BooleanValue(false));
    NetDeviceContainer staLeftDev = wifi.Install(phyLeft, mac, sender);


    // ---- Right: RouterR(AP) <--> Receiver(STA) ------------------------
    YansWifiPhyHelper phyRight;
    phyRight.SetChannel(MakeChannel().Create());

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("cerl-right")));
    NetDeviceContainer apRightDev = wifi.Install(phyRight, mac, routerR);

    mac.SetType("ns3::StaWifiMac",
                "Ssid",            SsidValue(Ssid("cerl-right")),
                "ActiveProbing",   BooleanValue(false));
    NetDeviceContainer staRightDev = wifi.Install(phyRight, mac, receiver);


    // ---- Wired backbone: RouterL <--> RouterR (bottleneck) ----------------
    PointToPointHelper bottle;
    bottle.SetDeviceAttribute ("DataRate", StringValue("5Mbps"));
    bottle.SetChannelAttribute("Delay",    StringValue("20ms"));
    bottle.SetQueue("ns3::DropTailQueue<Packet>",
                    "MaxSize", StringValue("50p"));
    NetDeviceContainer devLR = bottle.Install(routerL, routerR);
    
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(errorRate));
    em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    devLR.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    // IP addressing
    Ipv4AddressHelper addr;

    addr.SetBase("10.1.1.0", "255.255.255.0");
    addr.Assign(staLeftDev);
    addr.Assign(apLeftDev);

    addr.SetBase("10.2.1.0", "255.255.255.0");
    addr.Assign(devLR);

    addr.SetBase("10.3.1.0", "255.255.255.0");
    addr.Assign(apRightDev);
    Ipv4InterfaceContainer ifaceRight = addr.Assign(staRightDev);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();

    // Left
    pos->Add(Vector(0.0,  0.0, 0.0));   // sender   (STA left)
    pos->Add(Vector(10.0, 0.0, 0.0));   // routerL  (AP1) 

    // Right
    pos->Add(Vector(60.0, 0.0, 0.0));   // routerR  (AP2)
    pos->Add(Vector(65.0, 0.0, 0.0));   // receiver

    mobility.SetPositionAllocator(pos);

    // Sender, RouterL, RouterR — fixed
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sender);
    mobility.Install(routerL);
    mobility.Install(routerR);

    // Receiver 
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(receiver);
    receiver->GetObject<ConstantVelocityMobilityModel>()
        ->SetVelocity(Vector(velocity, 0.0, 0.0));

   
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications
    constexpr uint16_t PORT     = 9;
    Ipv4Address        recvAddr = ifaceRight.GetAddress(0); // receiver's WiFi iface

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

    
    Simulator::Schedule(Seconds(1.1), &ConnectCwndTrace, dir + "/cwnd.csv");

    ThroughputSampler sampler;
    sampler.Init(dir + "/throughput.csv", 0.5);
    sampler.Start(sink, 1.5);

    FlowMonitorHelper fmH;
    Ptr<FlowMonitor>  monitor = fmH.InstallAll();

    
    std::cout << "\n[CERL-WiFi] protocol=" << protocol
          << "  velocity=" << velocity << " m/s"
          << "  errorRate=" << errorRate * 100.0 << "%"
          << "  duration=" << duration << " s\n";


    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    monitor->CheckForLostPackets();

    std::ofstream csv(dir + "/flow_stats.csv");
    csv << "protocol,tx_packets,rx_packets,lost_packets,throughput_mbps,delay_ms\n";

    for (auto& [id, s] : monitor->GetFlowStats())
    {
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