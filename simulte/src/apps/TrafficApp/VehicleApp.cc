#include "VehicleApp.h"
#include "TrafficMsg_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include <fstream>
#include <algorithm>
#include <mutex>
using namespace inet;
Define_Module(VehicleApp);
// ═════════════════════════════════════════════════════════════
//  Statik CSV yazici — tum araclar tek dosyaya append eder.
//  DIKKAT: CSV analiz verisinin kaynagidir (sumoId, isEquipped, duration...).
//  ASLA KAPATILMAZ. Sadece traffic_log.txt (debug log) kapatildi.
// ═════════════════════════════════════════════════════════════
static const char* CSV_DIR =
    "/home/veins/my_workspace/test_workspace/simulte/simulations/cars/results";
static std::mutex csvMutex;
static bool       csvHeaderWritten = false;
static std::string cachedCsvPath;
static std::string getActiveConfigName()
{
    cConfigurationEx* cfg = getEnvir()->getConfigEx();
    const char* name = cfg ? cfg->getActiveConfigName() : nullptr;
    return (name && *name) ? std::string(name) : std::string("default");
}
static const std::string& getCsvPath()
{
    if (cachedCsvPath.empty()) {
        cachedCsvPath = std::string(CSV_DIR) + "/vehicle_stats_"
                        + getActiveConfigName() + ".csv";
    }
    return cachedCsvPath;
}
static void writeCsvHeader()
{
    std::ofstream f(getCsvPath(), std::ios::app);
    if (!f.is_open()) return;
    f << "vehicleId,sumoId,isEquipped,departTime,arrivalTime,duration,"
      << "routesReceived,routesApplied,routesFallback,laneChanges,departEdge,destEdge\n";
    f.flush();
}
static void appendCsvRow(const std::string& row)
{
    std::lock_guard<std::mutex> lock(csvMutex);
    const std::string& path = getCsvPath();
    if (!csvHeaderWritten) {
        std::ifstream check(path);
        bool exists = check.good() && check.peek() != std::ifstream::traits_type::eof();
        check.close();
        if (!exists) writeCsvHeader();
        csvHeaderWritten = true;
    }
    std::ofstream f(path, std::ios::app);
    if (f.is_open()) {
        f << row << "\n";
        f.flush();
    }
}
// ═════════════════════════════════════════════════════════════
//  log() — TUM LOGLAR KAPALI (hiz + disk icin)
//  traffic_log.txt artik yazilmaz. Govde bosaltildi: tum log(...)
//  cagrilari otomatik susar. Cagri satirlari kodda duruyor ama
//  hicbir sey yazilmaz. (CSV yazimi bundan BAGIMSIZ, aynen calisir.)
// ═════════════════════════════════════════════════════════════
void VehicleApp::log(const std::string& msg)
{
    (void)msg;   // kullanilmiyor — uyari engelleme
    return;      // LOG KAPALI
}
veins::TraCICommandInterface::Vehicle VehicleApp::getVehicle()
{
    return mobility->getCommandInterface()->vehicle(mobility->getExternalId());
}
static std::string getAttr(const std::string& line, const std::string& attr)
{
    std::string search = attr + "=\"";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = line.find("\"", pos);
    if (end == std::string::npos) return "";
    return line.substr(pos, end - pos);
}
void VehicleApp::parseEdgeIndex(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        log("HATA: net.xml acilamadi: " + filename);
        return;
    }
    std::vector<std::string> edgeIds;
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("<edge id=") == std::string::npos) continue;
        std::string id   = getAttr(line, "id");
        std::string func = getAttr(line, "function");
        if (func == "internal" || (!id.empty() && id[0] == ':')) continue;
        edgeIds.push_back(id);
    }
    file.close();
    std::sort(edgeIds.begin(), edgeIds.end());
    indexToEdge = edgeIds;
    for (int i = 0; i < (int)edgeIds.size(); i++) {
        edgeToIndex[edgeIds[i]] = i;
    }
    log("EDGE INDEX TABLOSU | " + std::to_string(edgeIds.size()) + " edge");
}
// [G2-B] realTT default 30 → -1.0 (server gecersiz oldugunu anlasin)
double VehicleApp::readRealTravelTime(const std::string& edgeId)
{
    try {
        return mobility->getCommandInterface()->road(edgeId).getCurrentTravelTime();
    }
    catch (...) {
        return -1.0;  // Server bunu outlier filtresinde atacak
    }
}
void VehicleApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        mobility = check_and_cast<veins::VeinsInetMobility*>(
            getParentModule()->getSubmodule("mobility")
        );
        reportInterval  = par("reportInterval").doubleValue();
        serverPort      = par("serverPort").intValue();
        localPort       = par("localPort").intValue();
        serverAddress   = par("serverAddress").stdstringValue();
        netXmlFile      = par("netXmlFile").stdstringValue();
        penetrationRate = par("penetrationRate").doubleValue();
        // [G2-D] Gereksiz random kaldirildi.
        // veinsManager.penetrationRate zaten araclari filtreliyor.
        // Burada yeniden random yapmak iki kat filtre uyguluyordu.
        // car[*].app[0].penetrationRate=1.0 ise bu modul olusan tum araclar icin equipped.
        isEquipped = (penetrationRate >= 1.0)
                     ? true
                     : (uniform(0, 1) < penetrationRate);
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localPort);
        socket.setCallback(this);
        parseEdgeIndex(netXmlFile);
        reportTimer = new cMessage("reportTimer");
        scheduleAt(simTime() + reportInterval, reportTimer);
        log("initialized | server=" + serverAddress +
            " | edges=" + std::to_string(indexToEdge.size()) +
            " | penetrationRate=" + std::to_string(penetrationRate) +
            " | isEquipped=" + std::to_string(isEquipped));
    }
    if (stage == inet::INITSTAGE_LAST) {
        resolvedServerAddr = inet::L3AddressResolver().resolve(serverAddress.c_str());
        log("serverAddr=" + resolvedServerAddr.str());
    }
}
void VehicleApp::handleMessage(cMessage* msg)
{
    if (msg == reportTimer) {
        sendReport();
        scheduleAt(simTime() + reportInterval, reportTimer);
        return;
    }
    if (socket.belongsToSocket(msg)) {
        socket.processMessage(msg);
        return;
    }
}
void VehicleApp::tryInitStats()
{
    if (statsInitialized) return;
    try {
        auto veh = getVehicle();
        std::string edgeId = veh.getRoadId();
        if (edgeId.empty() || edgeId[0] == ':') return;
        departTime       = simTime().dbl();
        departEdge       = edgeId;
        lastEdge         = edgeId;
        statsInitialized = true;
    } catch (...) {
    }
}
void VehicleApp::sendReport()
{
    tryInitStats();
    if (statsInitialized) {
        try {
            auto veh = getVehicle();
            std::string edgeId = veh.getRoadId();
            if (!edgeId.empty() && edgeId[0] != ':') {
                lastEdge = edgeId;
            }
        } catch (...) {}
    }
    if (!isEquipped) return;
    auto veh = getVehicle();
    std::string edgeId = veh.getRoadId();
    double speed       = veh.getSpeed();
    if (edgeId.empty() || edgeId[0] == ':') return;
    try {
        std::string currentLaneID = veh.getLaneId();
        if (!lastLaneID.empty() && currentLaneID != lastLaneID) {
            size_t lastUnderscore = lastLaneID.find_last_of('_');
            size_t currUnderscore = currentLaneID.find_last_of('_');
            if (lastUnderscore != std::string::npos && currUnderscore != std::string::npos) {
                std::string lastE = lastLaneID.substr(0, lastUnderscore);
                std::string currE = currentLaneID.substr(0, currUnderscore);
                if (lastE == currE) {
                    laneChangeCounter++;
                }
            }
        }
        lastLaneID = currentLaneID;
    } catch (...) {}
    auto it = edgeToIndex.find(edgeId);
    if (it == edgeToIndex.end()) return;
    int edgeIdx = it->second;
    auto planned = veh.getPlannedRoadIds();
    int destIdx = -1;
    if (!planned.empty()) {
        std::string destEdge = planned.back();
        auto dit = edgeToIndex.find(destEdge);
        if (dit != edgeToIndex.end()) {
            destIdx = dit->second;
        }
    }
    if (destIdx < 0) return;
    // [G2-B] Hedef edge kaydet — fallback icin lazim
    lastDestEdge = indexToEdge[destIdx];
    double realTT = readRealTravelTime(edgeId);
    auto report = inet::makeShared<VehicleReportMsg>();
    report->setEdgeIndex(edgeIdx);
    report->setDestEdgeIndex(destIdx);
    report->setSpeed(speed);
    report->setTimestamp(simTime().dbl());
    report->setRealTravelTime(realTT);
    report->setLaneChangeCount(laneChangeCounter);
    report->setChunkLength(inet::B(36));
    auto packet = new inet::Packet("VehicleReport");
    packet->insertAtBack(report);
    socket.sendTo(packet, resolvedServerAddr, serverPort);
    log("RAPOR | edge=" + edgeId + "[" + std::to_string(edgeIdx) + "]" +
        " | dest=" + indexToEdge[destIdx] +
        " | speed=" + std::to_string(speed) +
        " | realTT=" + std::to_string(realTT));
}
void VehicleApp::socketDataArrived(inet::UdpSocket* sock, inet::Packet* packet)
{
    if (!isEquipped) {
        delete packet;
        return;
    }
    auto routeMsg = packet->peekAtFront<ServerRouteMsg>();
    int routeLen = routeMsg->getRouteLength();
    if (routeLen <= 0 || routeLen > 64) {
        delete packet;
        return;
    }
    routesReceived++;
    std::list<std::string> edgeList;
    for (int i = 0; i < routeLen; i++) {
        int idx = routeMsg->getRouteEdges(i);
        if (idx < 0 || idx >= (int)indexToEdge.size()) {
            delete packet;
            return;
        }
        edgeList.push_back(indexToEdge[idx]);
    }
    auto veh = getVehicle();
    std::string currentEdge = veh.getRoadId();
    // ── Rotanin icinde mevcut edge'i ara ────────────────────
    std::list<std::string> trimmedRoute;
    bool found = false;
    for (const auto& e : edgeList) {
        if (e == currentEdge) found = true;
        if (found) trimmedRoute.push_back(e);
    }
    // ── [G2-A] Eger mevcut edge rotada YOKSA (LTE gecikmesi yuzunden
    //     arac ileri gitmis), changeTarget ile sadece hedefi guncelle.
    //     Boylece SUMO kendi rotasini hesaplar — sessiz fail yerine
    //     en azindan hedef korunmus olur.
    if (!found || trimmedRoute.size() < 2) {
        if (!edgeList.empty()) {
            std::string targetEdge = edgeList.back();
            try {
                veh.changeTarget(targetEdge);
                routesFallback++;
                log("ROTA FALLBACK | changeTarget=" + targetEdge +
                    " | currentEdge=" + currentEdge +
                    " (rota icinde bulunamadi)");
            } catch (...) {
                log("ROTA FALLBACK EXCEPTION | target=" + targetEdge);
            }
        }
        delete packet;
        return;
    }
    bool ok = veh.changeVehicleRoute(trimmedRoute);
    if (ok) {
        routesApplied++;
        log("ROTA UYGULANDI | " + std::to_string(trimmedRoute.size()) + " edge");
    } else {
        // [G2-A] changeVehicleRoute basarisiz olursa changeTarget ile son care
        std::string targetEdge = trimmedRoute.back();
        try {
            veh.changeTarget(targetEdge);
            routesFallback++;
            log("ROTA FALLBACK (route fail sonrasi) | target=" + targetEdge);
        } catch (...) {
            log("ROTA BASARISIZ EXCEPTION");
        }
    }
    delete packet;
}
void VehicleApp::writeStatsRow()
{
    if (!statsInitialized) return;
    double arrivalTime = simTime().dbl();
    double duration    = arrivalTime - departTime;
    std::string vehId = getParentModule()->getFullName();
    std::string sumoId;
    try {
        if (mobility) sumoId = mobility->getExternalId();
    } catch (...) {
        sumoId = "";
    }
    std::ostringstream row;
    row << vehId << ","
        << sumoId << ","
        << (isEquipped ? 1 : 0) << ","
        << departTime << ","
        << arrivalTime << ","
        << duration << ","
        << routesReceived << ","
        << routesApplied << ","
        << routesFallback << ","      // [G2-A] yeni sutun
        << laneChangeCounter << ","
        << departEdge << ","
        << lastEdge;
    appendCsvRow(row.str());
}
void VehicleApp::finish()
{
    writeStatsRow();
    cancelAndDelete(reportTimer);
    socket.close();
}
