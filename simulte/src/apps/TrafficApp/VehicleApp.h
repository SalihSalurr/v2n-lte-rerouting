#pragma once

#include <omnetpp.h>
#include "inet/common/INETDefs.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "veins_inet/VeinsInetMobility.h"
#include "veins/modules/mobility/traci/TraCICommandInterface.h"

#include <map>
#include <vector>
#include <string>

using namespace omnetpp;
using namespace veins;

class VehicleApp : public cSimpleModule, public inet::UdpSocket::ICallback
{
  protected:
    veins::VeinsInetMobility* mobility = nullptr;
    inet::UdpSocket socket;
    cMessage* reportTimer = nullptr;

    // ── Parametreler ────────────────────────────────────────
    double      reportInterval;
    int         serverPort;
    int         localPort;
    std::string serverAddress;
    std::string netXmlFile;
    inet::L3Address resolvedServerAddr;

    // ── Penetrasyon ─────────────────────────────────────────
    double penetrationRate;
    bool   isEquipped = false;

    // ── Edge index tablosu (net.xml'den, sunucuyla ayni) ────
    std::vector<std::string>       indexToEdge;
    std::map<std::string, int>     edgeToIndex;

    // ── Serit degisim takibi ────────────────────────────────
    std::string lastLaneID = "";
    int         laneChangeCounter = 0;

    // ── CSV icin istatistikler (YENI) ───────────────────────
    bool   statsInitialized = false;
    double departTime       = -1.0;
    std::string departEdge  = "";
    std::string lastEdge    = "";          // arrivalTime'da destEdge olarak kullanilacak
    int    routesReceived   = 0;
    int    routesApplied    = 0;
    int    routesFallback   = 0;
    std::string lastDestEdge = "";

    // ── Loglama ─────────────────────────────────────────────
    void log(const std::string& msg);

    // ── Lifecycle ───────────────────────────────────────────
    virtual void initialize(int stage) override;
    virtual int  numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage* msg) override;
    virtual void finish() override;

    // ── UDP callback'ler ────────────────────────────────────
    virtual void socketDataArrived(inet::UdpSocket* socket, inet::Packet* packet) override;
    virtual void socketErrorArrived(inet::UdpSocket* socket, inet::Indication* indication) override {}
    virtual void socketClosed(inet::UdpSocket* socket) override {}

    // ── Islevler ────────────────────────────────────────────
    void parseEdgeIndex(const std::string& filename);
    void sendReport();
    veins::TraCICommandInterface::Vehicle getVehicle();

    // ── Yardimcilar ─────────────────────────────────────────
    double readRealTravelTime(const std::string& edgeId);
    void   tryInitStats();         // departTime/departEdge ilk gercek edge'de doldur
    void   writeStatsRow();        // finish()'te CSV'ye yaz
};
