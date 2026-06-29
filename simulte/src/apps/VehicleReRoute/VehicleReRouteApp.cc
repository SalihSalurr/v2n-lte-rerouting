// ============================================================
//  VehicleReRouteApp.cc
//
//  Amaç:
//    Bu modül, Veins/TraCI API'si üzerinden araçlara runtime'da
//    rota değiştirme komutları göndermek için yazılmış bir test
//    uygulamasıdır. LTE V2N trafik yönlendirme tezi kapsamında
//    TraCI'nin sunduğu 5 farklı rota API'sini karşılaştırmak
//    amacıyla geliştirilmiştir.
//
//  Tetikleyici mekanizma:
//    Araç belirli bir edge'e (watchEdgeId) geldiğinde tetiklenir.
//    0.5s aralıklarla edge kontrolü yapılır (timerCheck döngüsü).
//    Her araç yalnızca bir kez reroute edilir (rerouteDone bayrağı).
//
//  Test sonuçları:
//    İstanbul OSM haritası, 3 eNodeB, penetrationRate=0.2
//    Simülasyon süresi: 75s, watchEdgeId: "826449813"
//    Tüm testlerde 13+ araç başarıyla reroute edildi.
//
//  Dosya konumu : src/apps/VehicleReRoute/VehicleReRouteApp.cc
//  NED dosyası  : simulations/cars/VehicleReRouteApp.ned
//  Derleme      : SimuLTE --deep Makefile otomatik bulur
// ============================================================

#include "VehicleReRouteApp.h"
#include <fstream>

Define_Module(VehicleReRouteApp);

// ============================================================
//  LOGGING
//
//  OMNeT++ Express modunda EV_INFO ve EV_DEBUG çıktıları
//  konsola yansımaz. Bu nedenle tüm loglar /tmp/reroute_log.txt
//  dosyasına yazılır. Her simülasyon öncesi dosyanın silinmesi
//  önerilir: rm -f /tmp/reroute_log.txt
//
//  Log formatı: [t=<simtime>] [<modül_adı>] <mesaj>
//  Örnek: [t=13.1] [car[0]] EDGE MATCH: 826449813
// ============================================================
void VehicleReRouteApp::log(const std::string& msg)
{
    std::ofstream f("/tmp/reroute_log.txt", std::ios::app);
    if (f.is_open()) {
        f << "[t=" << simTime() << "] ["
          << getParentModule()->getFullName() << "] "
          << msg << "\n";
        f.flush();
    }
}

// ============================================================
//  YARDIMCI: getVehicle()
//
//  TraCICommandInterface::Vehicle nesnesi döner.
//  Bu nesne üzerinden tüm TraCI araç komutları çağrılır.
//  mobility->getExternalId() → SUMO'daki araç kimliği (ör: "1000003.0")
// ============================================================
veins::TraCICommandInterface::Vehicle VehicleReRouteApp::getVehicle()
{
    return mobility->getCommandInterface()->vehicle(
        mobility->getExternalId()
    );
}

// ============================================================
//  INITIALIZE
//
//  INITSTAGE_APPLICATION_LAYER aşamasında çalışır.
//  Bu aşamada mobility modülü hazır, SUMO bağlantısı aktif.
//  Daha erken aşamalarda mobility henüz spawn olmamış olabilir.
//
//  omnetpp.ini'den okunan parametreler:
//    checkInterval       = 0.5s   (edge kontrol periyodu)
//    watchEdgeId         = "826449813"
//    targetEdgeId        = "5170334#0"
//    artificialTravelTime= 3600s
// ============================================================
void VehicleReRouteApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        // Mobility modülü Car modülünün doğrudan alt modülüdür
        mobility = check_and_cast<veins::VeinsInetMobility*>(
            getParentModule()->getSubmodule("mobility")
        );

        checkInterval        = par("checkInterval").doubleValue();
        targetEdgeId         = par("targetEdgeId").stdstringValue();
        watchEdgeId          = par("watchEdgeId").stdstringValue();
        artificialTravelTime = par("artificialTravelTime").doubleValue();

        // Edge kontrol döngüsünü başlat
        // Araç her checkInterval saniyede bir konumunu kontrol eder
        timerCheck = new cMessage("timerCheck");
        scheduleAt(simTime() + checkInterval, timerCheck);

        log("initialized | watchEdge=" + watchEdgeId);
    }
}

// ============================================================
//  HANDLE MESSAGE
//
//  timerCheck mesajı geldiğinde:
//    1. Aracın bulunduğu edge okunur (getRoadId)
//    2. watchEdgeId ile karşılaştırılır
//    3. Eşleşirse seçili rota API'si çağrılır
//    4. rerouteDone=true yapılır (tekrar tetiklenme önlenir)
//    5. Eşleşme yoksa timer yeniden planlanır
// ============================================================
void VehicleReRouteApp::handleMessage(cMessage* msg)
{
    if (msg != timerCheck) return;

    if (!rerouteDone) {
        std::string currentEdge = getVehicle().getRoadId();

        if (currentEdge == watchEdgeId) {
            log("EDGE MATCH: " + currentEdge);

            // .h dosyasındaki #define ile hangi API test edileceği seçilir
            // Sadece bir #define aktif olmalı, diğerleri yorum satırında olmalı
#ifdef REROUTE_MODE_NEW_ROUTE
            doReroute_newRoute();
#elif defined(REROUTE_MODE_CHANGE_ROUTE)
            doReroute_changeRoute();
#elif defined(REROUTE_MODE_CHANGE_VEHICLE)
            doReroute_changeVehicleRoute();
#elif defined(REROUTE_MODE_CHANGE_TARGET)
            doReroute_changeTarget();
#elif defined(REROUTE_MODE_GET_PLANNED)
            doReroute_getPlannedOnly();
#endif
            rerouteDone = true;
            return; // timer yeniden planlanmaz, döngü durur
        }
    }

    // Henüz hedef edge'e ulaşılmadı, bir sonraki kontrolü planla
    scheduleAt(simTime() + checkInterval, timerCheck);
}

// ============================================================
//  API 1: newRoute(edgeId)
//
//  Kullanım:
//    Sadece hedef edge ID verilir. SUMO, mevcut araç konumundan
//    hedefe Dijkstra algoritmasıyla en kısa rotayı hesaplar.
//    Ara edge'leri bilmene gerek yoktur.
//
//  Ne zaman kullan:
//    Sunucu sadece hedef noktayı bildiriyorsa.
//    En sade ve hızlı rota değiştirme yöntemi.
//
//  Test çıktısı (İstanbul haritası, car[0], t=13.1s):
//    [t=13.1] [car[0]] newRoute() ÖNCE | edge=826449813
//    [t=13.1] [car[0]] newRoute() SONRA | planned=
//      28585046#6 -> 1064645867 -> 23236879 -> 826449813
//      -> 23639695 -> 23639778 -> 5170334#0
//
//  Gözlem:
//    826449813'ten 5170334#0'a sadece 3 ara edge ile ulaşıldı.
//    SUMO optimal rotayı buldu. Tüm araçlar aynı rotayı aldı.
// ============================================================
void VehicleReRouteApp::doReroute_newRoute()
{
    auto veh = getVehicle();
    log("newRoute() ÖNCE | edge=" + veh.getRoadId());

    veh.newRoute(targetEdgeId);

    auto planned = veh.getPlannedRoadIds();
    std::string route;
    for (auto& e : planned) route += e + " -> ";
    log("newRoute() SONRA | planned=" + route);
}

// ============================================================
//  API 2: changeRoute(edgeId, travelTime)
//
//  Kullanım:
//    Belirli bir edge'in seyahat süresini yapay olarak artırır.
//    Araç, bu edge'i "çok yavaş" olarak algılayıp rotasını
//    yeniden hesaplar ve o edge'den KAÇINIR.
//    Doğrudan rota vermek yerine dolaylı yönlendirme yapar.
//
//  Parametreler:
//    edgeId     : yavaşlatılacak edge (genellikle tıkalı yol)
//    travelTime : yapay süre (saniye). 3600 = 1 saat → kesinlikle kaçınır
//
//  Ne zaman kullan:
//    Tıkalı yolları simüle ederken. Araçları belirli bir
//    bölgeden uzaklaştırmak istediğinde. Trafik yönetim
//    senaryolarında.
//
//  Test çıktısı (İstanbul haritası, car[0], t=13.1s):
//    [t=13.1] [car[0]] changeRoute() ÖNCE | edge=826449813
//    [t=13.1] [car[0]] changeRoute() SONRA | planned=
//      28585046#6 -> 1064645867 -> 23236879 -> 826449813
//      -> 827063322#0 -> 827063322#1 -> ... -> 83477315
//      (toplam 34 edge — 5170334#0'dan tamamen kaçındı)
//
//  Gözlem:
//    5170334#0 edge'i 3600s yapay süreyle "tıkalı" gösterildi.
//    SUMO tamamen farklı bir güzergah hesapladı (34 edge).
//    Her araç farklı alternatif rota buldu — deterministik değil.
//    newRoute ile kıyasla: 3 edge vs 34 edge farkı çarpıcı.
// ============================================================
void VehicleReRouteApp::doReroute_changeRoute()
{
    auto veh = getVehicle();
    log("changeRoute() ÖNCE | edge=" + veh.getRoadId());

    // 3600s = 1 saat yapay süre → araç bu edge'den kesinlikle kaçınır
    veh.changeRoute(targetEdgeId, artificialTravelTime);

    auto planned = veh.getPlannedRoadIds();
    std::string route;
    for (auto& e : planned) route += e + " -> ";
    log("changeRoute() SONRA | planned=" + route);
}

// ============================================================
//  API 3: changeVehicleRoute(edgeList)
//
//  Kullanım:
//    Tam edge listesi verilerek araç rotası atanır.
//    SUMO herhangi bir hesaplama yapmaz, verilen listeyi direkt uygular.
//    bool döner: true=başarılı, false=geçersiz/bağlantısız edge listesi.
//
//  Önemli kural:
//    Edge listesindeki her ardışık edge çifti haritada gerçekten
//    birbirine bağlı olmalıdır. Aksi halde SUMO hata verir:
//    "No connection between edge 'X' and edge 'Y'"
//    Doğru liste newRoute() çıktısından alınabilir.
//
//  Ne zaman kullan:
//    Sunucu tam rota listesi hesaplayıp araçlara gönderdiğinde.
//    Tezin V2N mimarisinde sunucu tarafında rota hesaplanıp
//    LTE üzerinden araçlara iletildiği senaryoda bu API kullanılır.
//    bool dönüş değeri sayesinde başarı/hata kontrolü mümkündür.
//
//  Test çıktısı (İstanbul haritası, car[0], t=13.1s):
//    [t=13.1] [car[0]] changeVehicleRoute() ÖNCE | edge=826449813
//    [t=13.1] [car[0]] changeVehicleRoute() BASARILI | planned=
//      28585046#6 -> 1064645867 -> 23236879 -> 826449813
//      -> 23639695 -> 23639778 -> 5170334#0
//
//  Gözlem:
//    newRoute ile aynı rotayı üretir (edge listesi aynı verildi).
//    Fark: bu API sunucudan gelen listeyi doğrudan uygular,
//    SUMO kendi hesaplamasını yapmaz. Deterministik ve kontrollü.
// ============================================================
void VehicleReRouteApp::doReroute_changeVehicleRoute()
{
    auto veh = getVehicle();
    log("changeVehicleRoute() ÖNCE | edge=" + veh.getRoadId());

    // UYARI: Tüm edge'ler haritada gerçekten birbirine bağlı olmalı.
    // Bu liste newRoute() test çıktısından alındı (826449813 → 5170334#0).
    // Gerçek V2N senaryosunda bu liste sunucudan LTE üzerinden gelir.
    std::list<std::string> newEdges = {
        "826449813",
        "23639695",
        "23639778",
        "5170334#0"
    };

    bool success = veh.changeVehicleRoute(newEdges);

    auto planned = veh.getPlannedRoadIds();
    std::string route;
    for (auto& e : planned) route += e + " -> ";
    log("changeVehicleRoute() " +
        std::string(success ? "BASARILI" : "BASARISIZ") +
        " | planned=" + route);
}

// ============================================================
//  API 4: changeTarget(edgeId)
//
//  Kullanım:
//    Aracın hedef edge'ini değiştirir. SUMO, mevcut konumdan
//    yeni hedefe Dijkstra ile rota hesaplar. newRoute'a işlevsel
//    olarak çok benzer, ancak iç implementasyonda mevcut route
//    nesnesini koruyarak sadece hedefi günceller.
//
//  newRoute ile farkı:
//    newRoute  → yeni bir route nesnesi oluşturur
//    changeTarget → mevcut route nesnesinin hedefini değiştirir
//    Pratik sonuç genellikle aynıdır, ancak SUMO iç davranışı farklıdır.
//
//  Veins versiyonu:
//    Veins 5.2+ gerektirir. Derleme hatası alınırsa
//    TraCICommandInterface.h içinde changeTarget() metodunu kontrol et.
//
//  Ne zaman kullan:
//    newRoute yerine tercih nedeni genellikle yoktur.
//    Route ID'sini korumak önemliyse bu API seçilir.
//
//  Test çıktısı (İstanbul haritası, car[0], t=13.1s):
//    [t=13.1] [car[0]] changeTarget() ÖNCE | edge=826449813
//    [t=13.1] [car[0]] changeTarget() SONRA | planned=
//      28585046#6 -> 1064645867 -> 23236879 -> 826449813
//      -> 23639695 -> 23639778 -> 5170334#0
//
//  Gözlem:
//    newRoute ile birebir aynı rota çıktısı üretildi.
//    İstanbul haritasında ikisi arasında pratik fark gözlemlenmedi.
// ============================================================
void VehicleReRouteApp::doReroute_changeTarget()
{
    auto veh = getVehicle();
    log("changeTarget() ÖNCE | edge=" + veh.getRoadId());

    veh.changeTarget(targetEdgeId);

    auto planned = veh.getPlannedRoadIds();
    std::string route;
    for (auto& e : planned) route += e + " -> ";
    log("changeTarget() SONRA | planned=" + route);
}

// ============================================================
//  API 5: getPlannedRoadIds()
//
//  Kullanım:
//    Aracın önündeki planlanmış tüm edge listesini döner.
//    Rota DEĞİŞTİRMEZ — sadece mevcut planı okur.
//    list<string> döner: her eleman bir edge ID'sidir.
//
//  Ne zaman kullan:
//    Diğer API'ları test etmeden önce baseline almak için.
//    Reroute öncesi ve sonrası karşılaştırma için.
//    Sunucuya gönderilecek araç rota bilgisini okumak için.
//    AoI (Age of Information) hesaplamalarında rota uzunluğunu
//    ölçmek için kullanılabilir.
//
//  Test çıktısı (İstanbul haritası, car[0], t=13.1s):
//    [t=13.1] [car[0]] getPlannedRoadIds() | toplam=35 |
//      28585046#6 -> 1064645867 -> 23236879 -> 826449813
//      -> 827063322#0 -> ... -> 83477315
//
//  Gözlem:
//    Araç henüz reroute edilmeden 35 edge'lik bir rotada.
//    Her araç farklı toplam edge sayısına sahip (16–35 arası).
//    Bu veri sunucuya gönderilebilecek temel trafik bilgisidir.
// ============================================================
void VehicleReRouteApp::doReroute_getPlannedOnly()
{
    auto veh = getVehicle();

    auto planned = veh.getPlannedRoadIds();
    std::string route;
    for (auto& e : planned) route += e + " -> ";

    log("getPlannedRoadIds() | toplam=" +
        std::to_string(planned.size()) +
        " | " + route);
}

// ============================================================
//  FINISH
//
//  Simülasyon bitişinde timer mesajı iptal edilir.
//  cancelAndDelete: hem iptal eder hem belleği serbest bırakır.
// ============================================================
void VehicleReRouteApp::finish()
{
    log("finish | rerouteDone=" + std::to_string(rerouteDone));
    cancelAndDelete(timerCheck);
}
