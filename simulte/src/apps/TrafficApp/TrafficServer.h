#ifndef __TRAFFICSERVER_H
#define __TRAFFICSERVER_H

#include <omnetpp.h>
#include <map>
#include <vector>
#include <string>
#include <queue>
#include <limits>
#include <cstdint>

#include "inet/common/INETDefs.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

using namespace omnetpp;

struct EdgeData {
    std::string id;
    int    index = -1;
    double length = 0.0;
    double speedLimit = 13.9;
    double baseTravelTime = 10.0;
};

struct EdgeTraffic {
    int    vehicleCount = 0;       // sadece kac ayri rapor geldi (decay'le yarilanir)
    double avgSpeed     = 13.9;    // EWMA
    double lastUpdate   = 0.0;
    double realTravelTime = 0.0;   // EWMA
    int    realTTCount    = 0;
};

struct VehicleEntry {
    inet::L3Address address;
    int             port;
    int             currentEdge = -1;
    int             destEdge    = -1;
};

class TrafficServer : public cSimpleModule, public inet::UdpSocket::ICallback
{
  protected:
    inet::UdpSocket socket;

    std::vector<EdgeData>              edges;
    std::map<std::string, int>         edgeNameToIdx;
    std::vector<std::vector<int>>      adjacency;
    std::vector<EdgeTraffic>           traffic;

    std::map<std::string, VehicleEntry> vehicleMap;
    std::map<std::string, double>       lastRouteSent;
    std::map<std::string, int>          lastRouteEdge;

    int    listenPort;
    int    vehicleBasePort;
    double K;
    double speedLimit;
    double normalTravelTime;
    double decayInterval;
    double rerouteInterval;
    double minRouteDistance;   // bu mesafenin altindaki rotalara mudahale yok (m)
    std::string netXmlFile;

    cMessage* decayTimer = nullptr;

    cMessage* rerouteCounterResetTimer = nullptr;
    int rerouteCountThisSecond = 0;
    void log(const std::string& msg);

    virtual void initialize(int stage) override;
    virtual int  numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage* msg) override;
    virtual void finish() override;

    virtual void socketDataArrived(inet::UdpSocket* socket, inet::Packet* packet) override;
    virtual void socketErrorArrived(inet::UdpSocket* socket, inet::Indication* indication) override {}
    virtual void socketClosed(inet::UdpSocket* socket) override {}

    void parseNetXml(const std::string& filename);

    // ── Stokastik routing icin vehicleSeed eklendi ──────────
    double getEdgeWeight(int edgeIdx, uint32_t vehicleSeed);
    std::vector<int> dijkstra(int srcEdge, int dstEdge, uint32_t vehicleSeed);

    void sendRoute(const std::string& vehicleKey, const std::vector<int>& route);
    void decayEdgeInfo();

    // Arac anahtarindan deterministik seed uretir
    uint32_t hashVehicleKey(const std::string& key);
};

#endif
