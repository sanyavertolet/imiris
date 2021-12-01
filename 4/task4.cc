#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

NS_LOG_COMPONENT_DEFINE("App");

uint64_t sended = 0;
uint64_t droped = 0;
double   q_mid_size = 0;
uint64_t q_max_size = 0;

class App: public ns3::Application {
public:
    App();
    virtual ~App();
    void Setup(ns3::Ptr<ns3::Socket> socket, ns3::Address address, uint32_t packetSize, std::default_random_engine *generator, std::exponential_distribution<double> *distribution, ns3::Ptr<ns3::Queue<ns3::Packet>> queue, std::string host_name);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    void PrintTime(void);
    void SendPacket(void);
    void ScheduleTx(void);
 

    bool mod_running;
    uint32_t mod_packetSize;
    uint32_t mod_packetsSent;
    
    ns3::Address mod_peer;
    ns3::Ptr<ns3::Socket> mod_socket;
    ns3::Ptr<ns3::Queue<ns3::Packet>> mod_queue;
    ns3::EventId mod_sendEvent;
    ns3::EventId mod_timeEvent;
    
    std::default_random_engine *mod_generator;
    std::exponential_distribution<double> *mod_distribution;
    std::string mod_name;
};

App::App(): mod_running(false), mod_packetSize(0), mod_packetsSent(0), mod_peer(), mod_socket(0), mod_sendEvent() {}

App::~App() {
    mod_socket = 0;
    delete mod_generator;
    delete mod_distribution;
}

void App::Setup(ns3::Ptr<ns3::Socket> socket, ns3::Address address, uint32_t packetSize, std::default_random_engine *generator, std::exponential_distribution<double> *distribution, ns3::Ptr<ns3::Queue<ns3::Packet>> queue, std::string host_name) {
    mod_socket = socket;
    mod_peer = address;
    mod_packetSize = packetSize;
    mod_generator = generator;
    mod_distribution = distribution;
    mod_queue = queue;
    mod_name = host_name;
}

void App::StartApplication(void) {
    mod_running = true;
    mod_packetsSent = 0;
    mod_socket->Bind();
    mod_socket->Connect(mod_peer);
    mod_socket->SetRecvCallback(ns3::MakeNullCallback<void, ns3::Ptr<ns3::Socket>>());
    mod_socket->SetAllowBroadcast(true);
    mod_sendEvent = ns3::Simulator::Schedule(ns3::Seconds(0.0), &App::SendPacket, this);
    mod_timeEvent = ns3::Simulator::Schedule(ns3::Seconds(0.0), &App::PrintTime, this);
}
void App::PrintTime(void) {
    if (mod_name == "Host 0") {
        NS_LOG_INFO("Current time: " << (ns3::Simulator::Now()).GetSeconds());
    }
    mod_timeEvent = ns3::Simulator::Schedule(ns3::Seconds(0.1), &App::PrintTime, this);
}
void App::StopApplication(void) {
    mod_running = false;
    if (mod_sendEvent.IsRunning()) {
        ns3::Simulator::Cancel(mod_sendEvent);
    }
    if (mod_socket) {
        mod_socket->Close();
    }
}
void App::SendPacket(void) {
    ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(mod_packetSize);
    mod_socket->Send(packet);
    ++sended;
    ScheduleTx();
}
void App::ScheduleTx(void) {
    if (mod_running) {
        ns3::Time tNext(ns3::MilliSeconds((*mod_distribution)(*mod_generator)*1000));
        q_mid_size += mod_queue->GetNPackets();
        q_max_size = q_max_size > mod_queue->GetNPackets()? q_max_size: mod_queue->GetNPackets();
        mod_sendEvent = ns3::Simulator::Schedule(tNext, &App::SendPacket, this);
    }
}
static void QueDrop(std::string context, ns3::Ptr<const ns3::Packet> p) {
    ++droped;
}

int main(int argc, char *argv[]) {
    uint64_t csma_num = 724;
    double alpha = 10;
    uint64_t delay = 1000;
    
    ns3::LogComponentEnable("App", ns3::LOG_LEVEL_INFO);

    ns3::NodeContainer nodes;
    nodes.Create(csma_num + 1);

    ns3::CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", ns3::TimeValue(ns3::NanoSeconds(delay)));
    csma.SetQueue("ns3::DropTailQueue");

    ns3::NetDeviceContainer devices = csma.Install(nodes);

    std::vector<ns3::Ptr<ns3::Queue<ns3::Packet>>> queues;
    for (uint32_t i = 0; i < csma_num; i++) {
        ns3::Ptr<ns3::RateErrorModel> em = ns3::CreateObject<ns3::RateErrorModel>();
        em->SetAttribute("ErrorRate", ns3::DoubleValue(0.00000001));
        ns3::Ptr<ns3::DropTailQueue<ns3::Packet>> que = ns3::CreateObject<ns3::DropTailQueue<ns3::Packet>>();
        que->SetMaxSize(ns3::QueueSize("10p"));
        que->TraceConnect("Drop", "Host " + std::to_string(i) + ": ", ns3::MakeCallback(&QueDrop));
        queues.push_back(que);
        devices.Get(i)->SetAttribute("TxQueue", ns3::PointerValue(que));
        devices.Get(i)->SetAttribute("ReceiveErrorModel", ns3::PointerValue(em));
    }

    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    ns3::Address sinkAddress(ns3::InetSocketAddress(interfaces.GetAddress(csma_num), sinkPort));
    ns3::PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), sinkPort));
    
    ns3::ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(csma_num));
    sinkApps.Start(ns3::Seconds(0.0));
    sinkApps.Stop(ns3::Seconds(10.0));

    for (uint32_t i = 0; i < csma_num; i++) {
        ns3::Ptr<ns3::Socket> ns3UdpSocket = ns3::Socket::CreateSocket(nodes.Get(i), ns3::UdpSocketFactory::GetTypeId());
        ns3::Ptr<App> app = ns3::CreateObject<App>();
	    app->Setup(ns3UdpSocket, sinkAddress, 1500, new std::default_random_engine(i), new std::exponential_distribution<double>(alpha), queues[i], "Host " + std::to_string(i));
        nodes.Get(i)->AddApplication(app);
        app->SetStartTime(ns3::Seconds(0.0));
        app->SetStopTime(ns3::Seconds(10.0));
    }

    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    q_mid_size /= sended;
    
    std::cout << "Alpha = " << alpha << std::endl;
    std::cout << "N = " << csma_num << std::endl;
    std::cout << "Send: " << sended << std::endl;
    std::cout << "Drop: " << droped << std::endl;
    std::cout << "Part of Droped packets: " << double(droped) / double(sended) << std::endl;
    std::cout << "Midle queue size: " << q_mid_size << std::endl;
    std::cout << "Max queue sizes: " << q_max_size << std::endl;

    return 0;
}
