#include "TrafficServer.h"
#include <fstream>
#include "TrafficMsg_m.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <limits>
#include <random>
#include <functional>

using namespace inet;

Define_Module(TrafficServer);

// ═════════════════════════════════════════════════════════════
//  Sabitler (algoritma parametreleri — kod icinde)
// ═════════════════════════════════════════════════════════════
static constexpr double EWMA_ALPHA       = 0.3;   // hiz/realTT EWMA agirligi
static constexpr double DECAY_MULT       = 0.92;  // [G3-A] cift-decay duzeltmesi: 0.85 → 0.92 (EWMA zaten yumusatiyor)
static constexpr double NOISE_AMPLITUDE  = 0.18;  // [G1-A] gercek ±%18 dagilim, alternatif rota cesitliligi
static constexpr double CONGESTION_THR   = 0.20;  // bu altinda mudahale yok
static constexpr double HYSTERESIS_RATIO = 1.0;   // [G2-B] edgeChanged tek basina tetiklemesin, full interval bekle

// [DUZELTME] BPR cost function KALDIRILDI — congestion 0-1 ile sinirli oldugu icin
// congestion^4 cezayi felc ediyordu (tipik tikali edge sadece %1.2 ceza aliyordu).
// Lineer cezaya donuldu: weight = baseTT * (1 + congestion * K). Bu sabitler artik
// KULLANILMIYOR, referans icin birakildi.
static constexpr double BPR_ALPHA        = 0.15;  // KULLANILMIYOR (eski BPR)
static constexpr double BPR_BETA         = 4.0;   // KULLANILMIYOR (eski BPR)

// [G1-C] realTT outlier filtresi: baseTravelTime'in N katindan buyukse outlier
static constexpr double REAL_TT_MAX_RATIO = 5.0;
static constexpr double REAL_TT_MIN_SEC   = 0.5;  // 0.5s'den kisa olculer atilir

// [G3-A] Yogunluk (density) icin referans deger
static constexpr double DENSITY_REF      = 0.10;  // arac/metre — bunun ustu tikanik kabul edilir

// [G3-B] Stagger reroute: ayni anda kac arac maks reroute alabilir (per second)
static constexpr int    MAX_REROUTES_PER_SEC = 8;

// ═════════════════════════════════════════════════════════════
//  log() — TUM LOGLAR KAPALI (hiz + disk icin)
//  Fonksiyon govdesi bosaltildi: tum log(...) cagrilari otomatik susar.
//  Cagri satirlari kodda duruyor ama hicbir sey yazilmaz.
// ═════════════════════════════════════════════════════════════
void TrafficServer::log(const std::string& msg)
{
    (void)msg;   // kullanilmiyor — uyari engelleme
    return;      // LOG KAPALI
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

// ═════════════════════════════════════════════════════════════
//  Arac anahtarindan deterministik seed (FNV-1a)
// ═════════════════════════════════════════════════════════════
uint32_t TrafficServer::hashVehicleKey(const std::string& key)
{
    uint32_t h = 2166136261u;
    for (char c : key) {
        h ^= (uint8_t)c;
        h *= 16777619u;
    }
    return h;
}

void TrafficServer::parseNetXml(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        log("HATA: net.xml acilamadi: " + filename);
        return;
    }

    std::string line;
    std::string currentEdgeId;
    bool inEdge = false;

    while (std::getline(file, line)) {
        if (line.find("<edge id=") != std::string::npos) {
            std::string id = getAttr(line, "id");
            std::string func = getAttr(line, "function");

            if (func == "internal" || (!id.empty() && id[0] == ':')) {
                inEdge = false;
                continue;
            }
            currentEdgeId = id;
            inEdge = true;

            if (edgeNameToIdx.find(id) == edgeNameToIdx.end()) {
                EdgeData ed;
                ed.id = id;
                edgeNameToIdx[id] = -1;
                edges.push_back(ed);
            }
        }
        else if (inEdge && line.find("<lane id=") != std::string::npos) {
            std::string lengthStr = getAttr(line, "length");
            std::string speedStr  = getAttr(line, "speed");

            if (!lengthStr.empty() && !speedStr.empty()) {
                double length = std::stod(lengthStr);
                double speed  = std::stod(speedStr);

                for (auto& ed : edges) {
                    if (ed.id == currentEdgeId && ed.length == 0.0) {
                        ed.length = length;
                        ed.speedLimit = speed;
                        ed.baseTravelTime = (speed > 0) ? length / speed : 9999.0;
                        break;
                    }
                }
            }
            inEdge = false;
        }
        else if (line.find("</edge>") != std::string::npos) {
            inEdge = false;
        }
    }

    std::sort(edges.begin(), edges.end(),
              [](const EdgeData& a, const EdgeData& b) { return a.id < b.id; });

    edgeNameToIdx.clear();
    for (int i = 0; i < (int)edges.size(); i++) {
        edges[i].index = i;
        edgeNameToIdx[edges[i].id] = i;
    }

    traffic.resize(edges.size());
    adjacency.resize(edges.size());

    file.clear();
    file.seekg(0);

    while (std::getline(file, line)) {
        if (line.find("<connection from=") == std::string::npos) continue;

        std::string dir = getAttr(line, "dir");
        if (dir == "t") continue;

        std::string fromEdge = getAttr(line, "from");
        std::string toEdge   = getAttr(line, "to");

        if (fromEdge.empty() || toEdge.empty()) continue;
        if (fromEdge[0] == ':' || toEdge[0] == ':') continue;

        auto itFrom = edgeNameToIdx.find(fromEdge);
        auto itTo   = edgeNameToIdx.find(toEdge);
        if (itFrom == edgeNameToIdx.end() || itTo == edgeNameToIdx.end()) continue;

        int fromIdx = itFrom->second;
        int toIdx   = itTo->second;

        auto& neighbors = adjacency[fromIdx];
        if (std::find(neighbors.begin(), neighbors.end(), toIdx) == neighbors.end()) {
            neighbors.push_back(toIdx);
        }
    }

    file.close();

    int totalConnections = 0;
    for (auto& v : adjacency) totalConnections += v.size();

    log("NET.XML PARSE TAMAM | edge=" + std::to_string(edges.size()) +
        " | connection=" + std::to_string(totalConnections));
}

// ═════════════════════════════════════════════════════════════
//  [DUZELTME] Lineer cost function + [G3-A] yogunluk + per-vehicle noise
//
//  ESKI (arizali): weight = baseTT * (1 + K * BPR_ALPHA * congestion^BPR_BETA)
//        → congestion 0-1 ile sinirli, ^4 cezayi felc ediyordu.
//          Tipik tikali edge (cong=0.48) sadece %1.2 ceza aliyordu.
//  YENI: weight = baseTT * (1 + congestion * K)
//        → Lineer, guclu, ongorulebilir. K dogrudan ceza katsayisi.
//          cong=0.48, K=1.5 → carpan 1.72 (%72 ceza).
//          cong=1.0 (kilitli) → carpan 1+K.
// ═════════════════════════════════════════════════════════════
double TrafficServer::getEdgeWeight(int edgeIdx, uint32_t vehicleSeed)
{
    const EdgeData& ed = edges[edgeIdx];
    const EdgeTraffic& tr = traffic[edgeIdx];

    double baseTT = (tr.realTTCount > 0 && tr.realTravelTime > 0)
                    ? tr.realTravelTime
                    : ed.baseTravelTime;

    // ── Hiza dayali kullanim orani (speed-based v/c) ──────
    double speedRatio = (ed.speedLimit > 0 && tr.avgSpeed > 0)
                        ? std::min(tr.avgSpeed / ed.speedLimit, 1.0)
                        : 1.0;
    double speedCongestion = 1.0 - speedRatio;

    // ── [G3-A] Yogunluga dayali kullanim orani (density-based) ──
    double density = (ed.length > 0)
                     ? (double)tr.vehicleCount / ed.length
                     : 0.0;
    double densityRatio = std::min(density / DENSITY_REF, 1.0);

    // ── AoI decay (eski veriyi sondur) ────────────────────
    double aoi = simTime().dbl() - tr.lastUpdate;
    double aoiDecay = std::exp(-0.1 * aoi);

    // ── Birlesik congestion: hiz ve yogunlugun maksimumu * AoI ──
    double congestion = std::max(speedCongestion, densityRatio) * aoiDecay;

    if (congestion < CONGESTION_THR) return baseTT;

    // ── [DUZELTME] Lineer cost function ───────────────────
    // weight = baseTT * (1 + congestion * K)
    double weight = baseTT * (1.0 + congestion * K);

    // ── Arac basina deterministik gurultu ───────────────────
    uint32_t h = vehicleSeed ^ (uint32_t)(edgeIdx * 2654435761u);
    h ^= h >> 16; h *= 0x85ebca6bu;
    h ^= h >> 13; h *= 0xc2b2ae35u;
    h ^= h >> 16;
    double r = (h & 0xFFFFFF) / (double)0x1000000; // [0, 1)
    double noise = 1.0 + (r - 0.5) * 2.0 * NOISE_AMPLITUDE; // [1-A, 1+A]
    weight *= noise;

    return weight;
}

// ═════════════════════════════════════════════════════════════
//  Dijkstra (vehicleSeed parametreli)
// ═════════════════════════════════════════════════════════════
std::vector<int> TrafficServer::dijkstra(int srcEdge, int dstEdge, uint32_t vehicleSeed)
{
    int N = edges.size();
    std::vector<double> dist(N, std::numeric_limits<double>::infinity());
    std::vector<int>    prev(N, -1);
    std::vector<bool>   visited(N, false);

    using PII = std::pair<double, int>;
    std::priority_queue<PII, std::vector<PII>, std::greater<PII>> pq;

    dist[srcEdge] = 0.0;
    pq.push({0.0, srcEdge});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (visited[u]) continue;
        visited[u] = true;

        if (u == dstEdge) break;

        for (int v : adjacency[u]) {
            if (visited[v]) continue;
            double w = getEdgeWeight(v, vehicleSeed);
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;
                pq.push({dist[v], v});
            }
        }
    }

    std::vector<int> path;
    if (dist[dstEdge] == std::numeric_limits<double>::infinity()) {
        return path;
    }

    for (int at = dstEdge; at != -1; at = prev[at]) {
        path.push_back(at);
    }
    std::reverse(path.begin(), path.end());

    return path;
}

// ═════════════════════════════════════════════════════════════
//  Initialize
// ═════════════════════════════════════════════════════════════
void TrafficServer::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        listenPort       = par("listenPort").intValue();
        vehicleBasePort  = par("vehicleBasePort").intValue();
        K                = par("K").doubleValue();
        speedLimit       = par("speedLimit").doubleValue();
        normalTravelTime = par("normalTravelTime").doubleValue();
        decayInterval    = par("decayInterval").doubleValue();
        rerouteInterval  = par("rerouteInterval").doubleValue();
        minRouteDistance = par("minRouteDistance").doubleValue();
        netXmlFile       = par("netXmlFile").stdstringValue();

        socket.setOutputGate(gate("socketOut"));
        socket.bind(listenPort);
        socket.setCallback(this);

        decayTimer = new cMessage("decayTimer");
        scheduleAt(simTime() + decayInterval, decayTimer);

        // [G3-B] Stagger reroute icin sayac sifirlama timer'i
        rerouteCounterResetTimer = new cMessage("rerouteCounterReset");
        scheduleAt(simTime() + 1.0, rerouteCounterResetTimer);

        parseNetXml(netXmlFile);

        log("initialized | port=" + std::to_string(listenPort) +
            " | K=" + std::to_string(K) +
            " | rerouteInterval=" + std::to_string(rerouteInterval) +
            " | decayMult=" + std::to_string(DECAY_MULT) +
            " | ewmaAlpha=" + std::to_string(EWMA_ALPHA) +
            " | noiseAmp=" + std::to_string(NOISE_AMPLITUDE) +
            " | costFunc=LINEAR(1+cong*K)" +
            " | maxReroutesPerSec=" + std::to_string(MAX_REROUTES_PER_SEC) +
            " | minRouteDist=" + std::to_string(minRouteDistance) + "m" +
            " | edges=" + std::to_string(edges.size()));
    }

    if (stage == inet::INITSTAGE_LAST) {
        inet::L3Address myAddr = inet::L3AddressResolver().resolve("server");
        log("myAddr=" + myAddr.str());
    }
}

void TrafficServer::handleMessage(cMessage* msg)
{
    if (msg == decayTimer) {
        decayEdgeInfo();
        scheduleAt(simTime() + decayInterval, decayTimer);
        return;
    }
    if (msg == rerouteCounterResetTimer) {
        // [G3-B] Her saniye reroute sayacini sifirla
        rerouteCountThisSecond = 0;
        scheduleAt(simTime() + 1.0, rerouteCounterResetTimer);
        return;
    }
    if (socket.belongsToSocket(msg)) {
        socket.processMessage(msg);
        return;
    }
}

// ═════════════════════════════════════════════════════════════
//  Arac raporu geldi
// ═════════════════════════════════════════════════════════════
void TrafficServer::socketDataArrived(inet::UdpSocket* sock, inet::Packet* packet)
{
    inet::L3Address srcAddr = packet->getTag<inet::L3AddressInd>()->getSrcAddress();
    int srcPort             = packet->getTag<inet::L4PortInd>()->getSrcPort();

    auto report = packet->peekAtFront<VehicleReportMsg>();

    int edgeIdx     = report->getEdgeIndex();
    int destIdx     = report->getDestEdgeIndex();
    double speed    = report->getSpeed();
    double realTT   = report->getRealTravelTime();

    if (edgeIdx < 0 || edgeIdx >= (int)edges.size() ||
        destIdx < 0 || destIdx >= (int)edges.size()) {
        delete packet;
        return;
    }

    // ── EWMA ile hiz guncellemesi ───────────────────────────
    EdgeTraffic& tr = traffic[edgeIdx];
    if (tr.vehicleCount == 0) {
        tr.avgSpeed = speed;
    } else {
        tr.avgSpeed = (1.0 - EWMA_ALPHA) * tr.avgSpeed + EWMA_ALPHA * speed;
    }
    tr.vehicleCount++;
    tr.lastUpdate = simTime().dbl();

    // ── [G1-C] realTT outlier filtresi ──────────────────────
    if (realTT > REAL_TT_MIN_SEC) {
        const EdgeData& ed = edges[edgeIdx];
        double maxAllowed = ed.baseTravelTime * REAL_TT_MAX_RATIO;
        if (realTT <= maxAllowed) {
            if (tr.realTTCount == 0) {
                tr.realTravelTime = realTT;
            } else {
                tr.realTravelTime = (1.0 - EWMA_ALPHA) * tr.realTravelTime + EWMA_ALPHA * realTT;
            }
            tr.realTTCount++;
        }
        // outlier'sa sessiz at — ne EWMA'ya ekle ne sayaca
    }

    // ── Arac kaydi ──────────────────────────────────────────
    std::string vehicleKey = srcAddr.str();
    vehicleMap[vehicleKey] = { srcAddr, srcPort, edgeIdx, destIdx };

    log("RAPOR | edge=" + edges[edgeIdx].id +
        " | dest=" + edges[destIdx].id +
        " | speed=" + std::to_string(speed) +
        " | avgSpeed=" + std::to_string(tr.avgSpeed) +
        " | realTT=" + std::to_string(realTT));

    if (edgeIdx == destIdx) {
        delete packet;
        return;
    }

    // ── Rota karari (hysteresis) ────────────────────────────
    double now = simTime().dbl();
    bool isFirstReport = (lastRouteSent.find(vehicleKey) == lastRouteSent.end());
    double sinceLast = isFirstReport ? 0.0 : (now - lastRouteSent[vehicleKey]);

    bool timeElapsed = !isFirstReport && (sinceLast >= rerouteInterval);

    // [G2-B] HYSTERESIS_RATIO=1.0 → edgeChanged tek basina tetiklemez,
    // tam interval gecmesi sart. Oscillation engellenir.
    bool edgeChanged = !isFirstReport
                       && (lastRouteEdge.find(vehicleKey) != lastRouteEdge.end())
                       && (lastRouteEdge[vehicleKey] != edgeIdx)
                       && (sinceLast >= rerouteInterval * HYSTERESIS_RATIO);

    if (isFirstReport || timeElapsed || edgeChanged) {
        // [G3-B] Stagger reroute: bu saniyede limiti astiysak atla
        if (rerouteCountThisSecond >= MAX_REROUTES_PER_SEC) {
            log("STAGGER_LIMIT | -> " + vehicleKey +
                " | bu saniye limit=" + std::to_string(MAX_REROUTES_PER_SEC));
            delete packet;
            return;
        }

        uint32_t seed = hashVehicleKey(vehicleKey);
        std::vector<int> route = dijkstra(edgeIdx, destIdx, seed);
        if (!route.empty() && route.size() >= 2) {
            double routeDistance = 0.0;
            for (int idx : route) {
                if (idx >= 0 && idx < (int)edges.size()) {
                    routeDistance += edges[idx].length;
                }
            }
            if (routeDistance < minRouteDistance) {
                lastRouteSent[vehicleKey] = now;
                lastRouteEdge[vehicleKey] = edgeIdx;
                log("KISA_YOLCULUK_ATLA | -> " + vehicleKey +
                    " | mesafe=" + std::to_string(routeDistance) +
                    "m < esik=" + std::to_string(minRouteDistance) + "m");
            } else {
                sendRoute(vehicleKey, route);
                lastRouteSent[vehicleKey] = now;
                lastRouteEdge[vehicleKey] = edgeIdx;
                rerouteCountThisSecond++;  // [G3-B]
            }
        }
    }

    delete packet;
}

void TrafficServer::sendRoute(const std::string& vehicleKey,
                              const std::vector<int>& route)
{
    if (vehicleMap.find(vehicleKey) == vehicleMap.end()) return;

    const VehicleEntry& entry = vehicleMap[vehicleKey];

    int routeLen = std::min((int)route.size(), 64);

    auto cmd = inet::makeShared<ServerRouteMsg>();
    cmd->setRouteLength(routeLen);
    for (int i = 0; i < 64; i++) {
        cmd->setRouteEdges(i, (i < routeLen) ? route[i] : -1);
    }
    cmd->setChunkLength(inet::B(260));

    auto packet = new inet::Packet("ServerRoute");
    packet->insertAtBack(cmd);

    socket.sendTo(packet, entry.address, entry.port);

    std::string routeStr;
    for (int i = 0; i < std::min(routeLen, 5); i++) {
        if (i > 0) routeStr += " -> ";
        routeStr += edges[route[i]].id;
    }
    if (routeLen > 5) routeStr += " -> ...(" + std::to_string(routeLen) + " toplam)";

    log("ROTA GONDERILDI | -> " + vehicleKey +
        " | " + std::to_string(routeLen) + " edge | " + routeStr);
}

// ═════════════════════════════════════════════════════════════
//  Decay (G3-A: cift-decay duzeltmesi — 0.85 → 0.92, EWMA zaten yumusatiyor)
// ═════════════════════════════════════════════════════════════
void TrafficServer::decayEdgeInfo()
{
    for (size_t i = 0; i < traffic.size(); i++) {
        auto& tr = traffic[i];
        tr.vehicleCount = (int)(tr.vehicleCount * DECAY_MULT);
        tr.realTTCount  = (int)(tr.realTTCount  * DECAY_MULT);

        if (tr.vehicleCount == 0) {
            double sl = (i < edges.size() && edges[i].speedLimit > 0)
                        ? edges[i].speedLimit : 13.9;
            tr.avgSpeed = (1.0 - EWMA_ALPHA) * tr.avgSpeed + EWMA_ALPHA * sl;
        }
        if (tr.realTTCount == 0) {
            tr.realTravelTime = 0.0;
        }
    }
}

void TrafficServer::finish()
{
    log("=== BITTI | edge=" + std::to_string(edges.size()) + " ===");
    cancelAndDelete(decayTimer);
    if (rerouteCounterResetTimer) cancelAndDelete(rerouteCounterResetTimer);
    socket.close();
}
