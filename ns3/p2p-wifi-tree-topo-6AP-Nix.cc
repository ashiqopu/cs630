#include <cmath>
#include <iostream>
#include <sstream>
// ns3 includes
#include "ns3/log.h"
#include "ns3/point-to-point-star.h"
#include "ns3/constant-position-mobility-model.h"

#include "ns3/node-list.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/vector.h"
#include "ns3/ipv6-address-generator.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/csma-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/topology-read-module.h"
#include "ns3/ipv4-nix-vector-helper.h"
using namespace ns3;

Ptr<ConstantVelocityMobilityModel> cvmm;

int main(int argc, char *argv[]){

    std::string format ("Inet");
    std::string input ("scratch/Inet_tree_topo_6AP.txt");
    std::string phyMode ("DsssRate1Mbps");
    std::vector<std::string> weight;
    uint32_t numWifiAps = 6;

    // Pick a topology reader based in the requested format.
    TopologyReaderHelper topoHelp;
    topoHelp.SetFileName (input);
    topoHelp.SetFileType (format);
    Ptr<TopologyReader> inFile = topoHelp.GetTopologyReader ();

    NodeContainer nodes;

    if (inFile != 0)
    {
        nodes = inFile->Read ();
    }
 
    if (inFile->LinksSize () == 0)
    {
        return -1;
    }

    //OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4NixVectorHelper nixRouting;

    Ipv4ListRoutingHelper list;
    list.Add (staticRouting, 0);
    list.Add (nixRouting, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper (list);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");

    int totlinks = inFile->LinksSize ();

    NodeContainer* nc = new NodeContainer[totlinks];
    TopologyReader::ConstLinksIterator iter;
    int i = 0;

    for ( iter = inFile->LinksBegin (); iter != inFile->LinksEnd (); iter++, i++ )
    {
        nc[i] = NodeContainer (iter->GetFromNode (), iter->GetToNode ());
        weight.push_back(iter->GetAttribute("Weight"));
    }

    NetDeviceContainer* ndc = new NetDeviceContainer[totlinks];
    PointToPointHelper p2p;
    for (int i = 0; i < totlinks; i++)
    {
        p2p.SetChannelAttribute ("Delay", StringValue (weight[i]));
        p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
        ndc[i] = p2p.Install (nc[i]);
    }


    Ipv4InterfaceContainer* ipic = new Ipv4InterfaceContainer[totlinks];
    for (int i = 0; i < totlinks; i++)
    {
        ipic[i] = address.Assign (ndc[i]);
        address.NewNetwork ();
    }

    for (int i = 0; i < totlinks; i++){
        std::cout << "Link-" << i << "Address 0 -->" << ipic[i].GetAddress(0) << " Address 1 -->" << ipic[i].GetAddress(1) << std::endl;
    }

    uint32_t totalNodes = nodes.GetN ();

    std::cout << "totalNodes =" << totalNodes << std::endl;

    NodeContainer wifiApNodes;
    wifiApNodes.Add(nodes.Get(6));
    wifiApNodes.Add(nodes.Get(7));
    wifiApNodes.Add(nodes.Get(8));
    wifiApNodes.Add(nodes.Get(9));
    wifiApNodes.Add(nodes.Get(10));
    wifiApNodes.Add(nodes.Get(11));

    Ptr<Node> sta = CreateObject<Node>();

    std::vector<NetDeviceContainer> apDevices;

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(90));

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue (phyMode),
                                  "ControlMode", StringValue (phyMode));
    phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    NetDeviceContainer staDevices;
    double wifiX = 100.0;
    int bottomrow = 5;            // number of bottom-row nodes
    int spacing = 200;            // between bottom-row nodes
    int mheight = 150;            // height of mover above bottom row
    int brheight = 50;            // height of bottom row

    int X = (bottomrow-1)*spacing+1;
    int endtime = (int)100*1.0;
    double speed = (X-1.0)/endtime;

    for (uint32_t i = 0; i < numWifiAps; i++){

        NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
        Ssid ssid = Ssid ("ns-3-ssid");
        /*std::ostringstream oss;
        oss << "wifi-default-" << i;
        Ssid ssid = Ssid (oss.str ());*/

        MobilityHelper mobility;

        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
        positionAlloc->Add(Vector(wifiX, brheight, 0.0));

        mobility.SetPositionAllocator (positionAlloc);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

        //setup AP
        mobility.Install (wifiApNodes.Get(i));

        mac.SetType ("ns3::ApWifiMac"
                    ,"Ssid", SsidValue (ssid)
                    );

        NetDeviceContainer apDev;

        apDev = wifi.Install (phy, mac, wifiApNodes.Get(i));

        apDevices.push_back(apDev);

        //setup one wifi station 
        if (i == 0)
        {
            Vector pos (-20, 10+brheight, 0);
            Vector vel (speed, 0, 0);
            MobilityHelper mobile;
            mobile.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
            mobile.Install(sta);
            cvmm = sta->GetObject<ConstantVelocityMobilityModel> ();
            cvmm->SetPosition(pos);
            cvmm->SetVelocity(vel);
            std::cout << "position: " << cvmm->GetPosition() << " velocity: " << cvmm->GetVelocity() << std::endl;
            std::cout << "mover mobility model: " << mobile.GetMobilityModelType() << std::endl; // just for confirmation

            mac.SetType ("ns3::StaWifiMac",
                         "Ssid", SsidValue (ssid),
                         "ActiveProbing", BooleanValue (false));
            staDevices = wifi.Install (phy, mac, sta);

        }
        wifiX += 200.0;
    }

    internet.Install(sta);
    Ipv4InterfaceContainer wifiStaInterfaces;
    std::vector<Ipv4InterfaceContainer> apInterfaces;

    for (uint32_t i = 0; i < numWifiAps; i++){
        apInterfaces.push_back(address.Assign(apDevices[i].Get(0)));
        if (i == 0){
            wifiStaInterfaces.Add(address.Assign(staDevices.Get(0)));
        }

        //address.NewNetwork ();
    }


    for (uint32_t i = 0; i < numWifiAps; i++){
        std::cout<< "AP-" << i << " address --> " << apInterfaces[i].GetAddress(0)<< std::endl;
    }
    
    std::cout<< "STA  address --> " << wifiStaInterfaces.GetAddress(0)<< std::endl;

    p2p.EnablePcapAll ("non leaf");
    phy.EnablePcap ("wifi_AP1", apDevices[0].Get (0));
    phy.EnablePcap ("wifi_AP2", apDevices[1].Get (0));
    phy.EnablePcap ("wifi_AP3", apDevices[2].Get (0));
    phy.EnablePcap ("wifi_AP4", apDevices[3].Get (0));
    phy.EnablePcap ("wifi_AP5", apDevices[4].Get (0));
    phy.EnablePcap ("wifi_AP6", apDevices[5].Get (0));
    phy.EnablePcap ("wifi_sta", staDevices.Get (0));

    uint16_t port = 50000;
    Address hubLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", hubLocalAddress);
    ApplicationContainer staApp = packetSinkHelper.Install (sta);
    //ApplicationContainer staApp = packetSinkHelper.Install (nodes.Get(6));
    staApp.Start (Seconds (1.0));
    staApp.Stop (Seconds (240.0));

    OnOffHelper onOffHelper ("ns3::TcpSocketFactory", Address ());
    onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer rootApp;

    //AddressValue remoteAddress (InetSocketAddress (wifiStaInterfaces.GetAddress(0), port));
    AddressValue remoteAddress (InetSocketAddress ("10.1.13.2", port));
    onOffHelper.SetAttribute ("Remote", remoteAddress);
    rootApp.Add (onOffHelper.Install (nodes.Get(0)));
    rootApp.Start (Seconds (1.0));
    rootApp.Stop (Seconds (240.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream ("p2p-tree-wifi.tr"));
    Simulator::Stop (Seconds (240.0));
    Simulator::Run ();
    Simulator::Destroy ();

    delete[] ipic;
    delete[] ndc;
    delete[] nc;
 
    return 0;
}
