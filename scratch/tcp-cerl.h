#ifndef TCP_CERL_H
#define TCP_CERL_H

#include "ns3/tcp-linux-reno.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include <deque>
#include <numeric>
#include <cmath>
#include <algorithm>

namespace ns3 {
    NS_LOG_COMPONENT_DEFINE("TcpCerl");
struct CerlState
{
    double   m_T_s        {1e9};  // RTT_min
    double   m_l_bytes    {0.0};  // current queue-length estimate l
    double   m_lMax_bytes {0.0};  // l_max
    double   m_bw_bps     {5e6}; // bottleneck B 
    double   m_A          {0.55};// threshold constant

    int64_t  m_oldCwnd_bytes       {-1};   // -1 = not set
    uint32_t m_lastDecMaxSentSeqno {0};
    bool     m_lastDecValid        {false};

    virtual void onRttSample(double rttSec)
    {
        if (rttSec <= 0.0) return;
        updateT(rttSec);
        updateL(rttSec);
    }

    virtual bool isCongestion() const
    {
        double N = m_A * m_lMax_bytes;
        return (m_l_bytes >= N);
    }

    virtual ~CerlState() = default;

protected:
    void updateT(double rttSec)
    {
        if (rttSec < m_T_s) m_T_s = rttSec;
    }

    void updateL(double rttSec)
    {
        double excess = rttSec - m_T_s;          
        m_l_bytes = excess * (m_bw_bps / 8.0);
        if (m_l_bytes > m_lMax_bytes) m_lMax_bytes = m_l_bytes;
    }
};

struct CerlStateAdaptive : public CerlState
{
    static constexpr size_t RTT_WINDOW_SIZE {32}; // number of samples

    double m_ABase {0.55};  
    std::deque<double> m_rttWindow; 

    void onRttSample(double rttSec) override
    {
        if (rttSec <= 0.0) return;

        m_rttWindow.push_back(rttSec);
        if (m_rttWindow.size() > RTT_WINDOW_SIZE)
            m_rttWindow.pop_front();

        updateT(rttSec);
        updateL(rttSec);

        if (m_rttWindow.size() >= 2)
        {
            double mu    = computeMean();
            double sigma = computeStdDev(mu);

            double ratio = (mu > 0.0) ? (sigma / mu) : 0.0;

            ratio = std::min(ratio, 1.0);

            m_A = m_ABase * (1.0 - ratio);
        }
        else
        {
            m_A = m_ABase; 
        }
    }

private:
    double computeMean() const
    {
        double sum = std::accumulate(m_rttWindow.begin(), m_rttWindow.end(), 0.0);
        return sum / static_cast<double>(m_rttWindow.size());
    }

    double computeStdDev(double mean) const
    {
        double sq = 0.0;
        for (double v : m_rttWindow)
        {
            double d = v - mean;
            sq += d * d;
        }
        return std::sqrt(sq / static_cast<double>(m_rttWindow.size()));
    }
};


struct CerlStateMobile : public CerlState
{
    static constexpr double WINDOW_SEC {5.0}; // sliding window width (s)

    // Timestamped RTT sample
    struct RttEntry { double timeSec; double rttSec; };
    std::deque<RttEntry> m_rttHistory;

    void onRttSample(double rttSec) override
    {
        if (rttSec <= 0.0) return;

        double now = Simulator::Now().GetSeconds();

        m_rttHistory.push_back({now, rttSec});

        while (!m_rttHistory.empty() &&
               (now - m_rttHistory.front().timeSec) > WINDOW_SEC)
        {
            m_rttHistory.pop_front();
        }

        double windowMin = 1e9;
        for (const auto& e : m_rttHistory)
            if (e.rttSec < windowMin) windowMin = e.rttSec;

        m_T_s = windowMin;  

        updateL(rttSec);
    }
};


template <typename StateT>
class TcpCerlBase : public TcpLinuxReno
{
public:
    TcpCerlBase() : TcpLinuxReno() {}
    TcpCerlBase(const TcpCerlBase& o)
        : TcpLinuxReno(o), m_cerl(o.m_cerl), m_inRecovery(o.m_inRecovery)
    {}
    ~TcpCerlBase() override = default;

    void PktsAcked(Ptr<TcpSocketState> tcb,
                   uint32_t            segmentsAcked,
                   const Time&         rtt) override
    {
        if (rtt.IsPositive())
            m_cerl.onRttSample(rtt.GetSeconds());

        TcpLinuxReno::PktsAcked(tcb, segmentsAcked, rtt);
    }

    void CongestionStateSet(Ptr<TcpSocketState>                  tcb,
                            const TcpSocketState::TcpCongState_t newState) override
    {
        if (newState == TcpSocketState::CA_RECOVERY)
        {
            m_inRecovery = true;
        }
        else if (newState == TcpSocketState::CA_LOSS)
        {
            m_inRecovery           = false;
            m_cerl.m_oldCwnd_bytes = -1;
        }
        else if (newState == TcpSocketState::CA_OPEN)
        {
            if (m_inRecovery && m_cerl.m_oldCwnd_bytes > 0)
            {
                uint32_t restored = static_cast<uint32_t>(m_cerl.m_oldCwnd_bytes);
                NS_LOG_INFO(GetName() << " deflation: restoring cwnd=" << restored);
                tcb->m_cWnd = restored;
            }

            m_inRecovery           = false;
            m_cerl.m_oldCwnd_bytes = -1;
        }

        TcpLinuxReno::CongestionStateSet(tcb, newState);
    }

    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb,
                         uint32_t                  bytesInFlight) override
    {
        if (!m_inRecovery)
            return TcpLinuxReno::GetSsThresh(tcb, bytesInFlight);

        uint32_t cwnd    = tcb->m_cWnd;
        uint32_t seg     = tcb->m_segmentSize;
        uint32_t highTx  = tcb->m_highTxMark.Get().GetValue();
        uint32_t lastAck = tcb->m_lastAckedSeq.GetValue();

        bool isCong          = m_cerl.isCongestion();
        bool isFirstDecrease = !m_cerl.m_lastDecValid ||
                               (lastAck - 1) > m_cerl.m_lastDecMaxSentSeqno;

        if (isCong && isFirstDecrease)
        {
            NS_LOG_INFO(GetName() << ": CONGESTION loss, halving cwnd=" << cwnd);
            m_cerl.m_lastDecMaxSentSeqno = highTx;
            m_cerl.m_lastDecValid        = true;
            m_cerl.m_oldCwnd_bytes       = -1;
            return std::max(cwnd / 2, 2 * seg);
        }
        else
        {
            NS_LOG_INFO(GetName() << ": RANDOM/non-first loss, preserving cwnd=" << cwnd);
            m_cerl.m_oldCwnd_bytes = static_cast<int64_t>(cwnd);
            return cwnd;
        }
    }

protected:
    StateT m_cerl;
    bool   m_inRecovery {false};
};



// Concrete TCP variants

// --- Baseline CERL 
class TcpCerl : public TcpCerlBase<CerlState>
{
public:
    static TypeId GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::TcpCerl")
                .SetParent<TcpLinuxReno>()
                .SetGroupName("Internet")
                .AddConstructor<TcpCerl>();
        return tid;
    }
    TcpCerl() = default;
    TcpCerl(const TcpCerl& o) : TcpCerlBase<CerlState>(o) {}
    std::string GetName() const override { return "TcpCerl"; }
    Ptr<TcpCongestionOps> Fork() override { return CopyObject<TcpCerl>(this); }
};

// --- A-CERL 
class TcpCerlA : public TcpCerlBase<CerlStateAdaptive>
{
public:
    static TypeId GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::TcpCerlA")
                .SetParent<TcpLinuxReno>()
                .SetGroupName("Internet")
                .AddConstructor<TcpCerlA>();
        return tid;
    }
    TcpCerlA() = default;
    TcpCerlA(const TcpCerlA& o) : TcpCerlBase<CerlStateAdaptive>(o) {}
    std::string GetName() const override { return "TcpCerlA"; }
    Ptr<TcpCongestionOps> Fork() override { return CopyObject<TcpCerlA>(this); }
};

// --- M-CERL 
class TcpCerlM : public TcpCerlBase<CerlStateMobile>
{
public:
    static TypeId GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::TcpCerlM")
                .SetParent<TcpLinuxReno>()
                .SetGroupName("Internet")
                .AddConstructor<TcpCerlM>();
        return tid;
    }
    TcpCerlM() = default;
    TcpCerlM(const TcpCerlM& o) : TcpCerlBase<CerlStateMobile>(o) {}
    std::string GetName() const override { return "TcpCerlM"; }
    Ptr<TcpCongestionOps> Fork() override { return CopyObject<TcpCerlM>(this); }
};

NS_OBJECT_ENSURE_REGISTERED(TcpCerl);
NS_OBJECT_ENSURE_REGISTERED(TcpCerlA);
NS_OBJECT_ENSURE_REGISTERED(TcpCerlM);

}
#endif 