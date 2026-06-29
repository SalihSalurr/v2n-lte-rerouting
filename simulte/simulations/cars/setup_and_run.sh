#!/bin/bash
# ============================================================
# MoST (Monaco) — Sifirdan Kurulum + Calistirma
# Kullanim: ./setup_and_run.sh Penetration15
#           ./setup_and_run.sh Penetration30
#           ./setup_and_run.sh Baseline
# ============================================================
set -e

SCENARIO="${1:-Penetration15}"
PROJECT_DIR="$HOME/my_workspace/test_workspace/simulte/simulations/cars"
SUMO_TOOLS="$HOME/src/sumo/tools"
VEHICLE_COUNT=10000
SIM_DURATION=3600

echo "============================================"
echo "  MoST Kurulum + Calistirma"
echo "  Senaryo: $SCENARIO"
echo "  Baslangic: $(date)"
echo "============================================"

# ════════════════════════════════════════════════
#  ADIM 1: Eski dosyalari temizle
# ════════════════════════════════════════════════
echo ""
echo "[1/8] Eski dosyalar temizleniyor..."
cd "$PROJECT_DIR"
rm -f istanbul-v1* slot1* slot2*
rm -f bologna*
rm -f most_trips.xml most.rou.alt.xml
echo "    Temizlendi."

# ════════════════════════════════════════════════
#  ADIM 2: MoST reposunu indir
# ════════════════════════════════════════════════
echo ""
echo "[2/8] MoST senaryosu indiriliyor..."
MOST_SRC="/tmp/MoSTScenario/scenario/in"
if [ ! -f "$MOST_SRC/most.net.xml" ]; then
    cd /tmp && rm -rf MoSTScenario
    git clone https://github.com/lcodeca/MoSTScenario.git --depth 1 2>&1 | tail -1
fi
if [ ! -f "$MOST_SRC/most.net.xml" ]; then
    echo "HATA: MoST indirilemedi!"; exit 1
fi
echo "    MoST hazir."

# ════════════════════════════════════════════════
#  ADIM 3: Harita dosyasini kopyala
# ════════════════════════════════════════════════
echo ""
echo "[3/8] Harita kopyalaniyor..."
cd "$PROJECT_DIR"
cp "$MOST_SRC/most.net.xml" ./most.net.xml
[ -f "$MOST_SRC/add/most.poly.xml" ] && cp "$MOST_SRC/add/most.poly.xml" ./most.poly.xml
echo "    most.net.xml kopyalandi ($(du -h most.net.xml | cut -f1))."

# ════════════════════════════════════════════════
#  ADIM 4: 10k arac uret
# ════════════════════════════════════════════════
echo ""
echo "[4/8] $VEHICLE_COUNT arac uretiliyor..."

RANDOM_TRIPS=""
for candidate in \
    "$SUMO_TOOLS/randomTrips.py" \
    "$HOME/src/sumo-1.11.0/tools/randomTrips.py" \
    "$(which sumo 2>/dev/null | sed 's|/bin/sumo|/tools/randomTrips.py|')"; do
    [ -f "$candidate" ] && RANDOM_TRIPS="$candidate" && break
done
[ -z "$RANDOM_TRIPS" ] && echo "HATA: randomTrips.py bulunamadi!" && exit 1

DEPART_PERIOD=$(python3 -c "print(round($SIM_DURATION/$VEHICLE_COUNT, 4))")

python3 "$RANDOM_TRIPS" \
    -n most.net.xml -o most_trips.xml \
    -b 0 -e "$SIM_DURATION" -p "$DEPART_PERIOD" \
    --vehicle-class passenger --min-distance 2000 \
    --validate --random 2>&1 | tail -3

echo "    Route hesaplaniyor (duarouter)..."
duarouter -n most.net.xml --route-files most_trips.xml -o most.rou.xml \
    --ignore-errors --repair 2>&1 | tail -3

rm -f most_trips.xml most.rou.alt.xml
ACTUAL=$(grep -c '<vehicle ' most.rou.xml)
echo "    $ACTUAL arac olusturuldu."

# ════════════════════════════════════════════════
#  ADIM 5: SUMO config dosyalari
# ════════════════════════════════════════════════
echo ""
echo "[5/8] SUMO config dosyalari olusturuluyor..."

cat > most.sumo.cfg << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<configuration xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://sumo.dlr.de/xsd/sumoConfiguration.xsd">
    <input>
        <net-file value="most.net.xml"/>
        <route-files value="most.rou.xml"/>
    </input>
    <time>
        <begin value="0"/>
        <step-length value="1.0"/>
    </time>
    <processing>
        <extrapolate-departpos value="true"/>
        <ignore-junction-blocker value="15"/>
        <collision.mingap-factor value="0.1"/>
        <collision.action value="none"/>
        <time-to-teleport value="120"/>
        <random-depart-offset value="0.001"/>
        <ignore-route-errors value="true"/>
    </processing>
    <routing>
        <weights.priority-factor value="2"/>
        <device.rerouting.probability value="0"/>
    </routing>
    <report>
        <verbose value="true"/>
        <duration-log.statistics value="true"/>
        <no-step-log value="true"/>
    </report>
    <random_number>
        <random value="false"/>
        <seed value="0"/>
    </random_number>
</configuration>
EOF

cat > most.launchd.xml << 'EOF'
<?xml version="1.0"?>
<launch>
    <copy file="most.net.xml" />
    <copy file="most.rou.xml" />
    <copy file="most.sumo.cfg" type="config" />
</launch>
EOF

echo "    most.sumo.cfg + most.launchd.xml olusturuldu."

# ════════════════════════════════════════════════
#  ADIM 6: omnetpp.ini
# ════════════════════════════════════════════════
echo ""
echo "[6/8] omnetpp.ini olusturuluyor..."

cat > omnetpp.ini << 'EOF'
[General]
cmdenv-express-mode = true
cmdenv-autoflush = true
image-path = ../../images
network = lte.simulations.cars.Highway
*.configurator.config = xmldoc("demo.xml")

debug-on-errors = false
*.veinsManager.ignoreGuiCommands = true
cmdenv-stop-batch-on-error = false

##########################################################
#            Simulation parameters                       #
##########################################################
debug-on-errors = false
print-undisposed = false
sim-time-limit = 5000s

**.scalar-recording = false
**.vector-recording = false
**.coreDebug = false
**.routingRecorder.enabled = false
**.tolerateMaxDistViolation = true

# ── eNodeB Fiziksel Koordinatlar (Monaco: 9975x6362m) ──
*.eNodeB1.mobility.initialX = 2500m
*.eNodeB1.mobility.initialY = 1600m
*.eNodeB1.mobility.initialZ = 30m

*.eNodeB2.mobility.initialX = 7500m
*.eNodeB2.mobility.initialY = 1600m
*.eNodeB2.mobility.initialZ = 30m

*.eNodeB3.mobility.initialX = 2500m
*.eNodeB3.mobility.initialY = 4800m
*.eNodeB3.mobility.initialZ = 30m

*.eNodeB4.mobility.initialX = 7500m
*.eNodeB4.mobility.initialY = 4800m
*.eNodeB4.mobility.initialZ = 30m

*.eNodeB*.mobilityType = "StationaryMobility"

*.playgroundSizeX = 10100m
*.playgroundSizeY = 6500m
*.playgroundSizeZ = 50m
*.annotations.draw = true

##########################################################
#            VeinsManager parameters                     #
##########################################################
*.veinsManager.host = "localhost"
*.veinsManager.port = 9999
*.veinsManager.autoShutdown = false
*.veinsManager.moduleType = "lte.corenetwork.nodes.cars.Car"
*.veinsManager.moduleName = "car"
*.veinsManager.launchConfig = xmldoc("most.launchd.xml")
*.veinsManager.drawRoads = false
*.veinsManager.updateInterval = 1s
*.veinsManager.firstStepAt = 0.1s

##########################################################
#                      Mobility                          #
##########################################################
*.car[*].mobilityType = "VeinsInetMobility"
*.car[*].ipv4.configurator.addressBase = "10.0.0.0"
*.car[*].ipv4.configurator.netmask = "255.0.0.0"

##########################################################
#              LTE specific parameters                   #
##########################################################
*.car[*].lteNic.phy.dynamicCellAssociation = true

**.car[*].masterId = 1
**.car[*].macCellId = 1

**.eNodeB1.macCellId = 1
**.eNodeB1.macNodeId = 1
**.eNodeB2.macCellId = 2
**.eNodeB2.macNodeId = 2
**.eNodeB3.macCellId = 3
**.eNodeB3.macNodeId = 3
**.eNodeB4.macCellId = 4
**.eNodeB4.macNodeId = 4
**.eNodeBCount = 4

**.rbAllocationType = "localized"
**.feedbackType = "ALLBANDS"
**.feedbackGeneratorType = "IDEAL"
**.maxHarqRtx = 2
**.numUe = ${numUEs=15000}

**.cellInfo.ruRange = 50
**.cellInfo.ruTxPower = "50,50,50;"
**.cellInfo.antennaCws = "2;"
**.cellInfo.numRbDl = 100
**.cellInfo.numRbUl = 100
**.numBands = 100
**.fbDelay = 1

*.car[*].lteNic.phy.enableHandover = false
*.eNodeB*.lteNic.phy.enableHandover = false
*.eNodeB*.lteNic.phy.broadcastMessageInterval = 0.5s

# X2 — 4 eNodeB tam mesh
*.eNodeB*.numX2Apps = 3
*.eNodeB*.x2App[*].server.localPort = 5000 + ancestorIndex(1)

*.eNodeB1.x2App[0].client.connectAddress = "eNodeB2%x2ppp0"
*.eNodeB1.x2App[1].client.connectAddress = "eNodeB3%x2ppp0"
*.eNodeB1.x2App[2].client.connectAddress = "eNodeB4%x2ppp0"

*.eNodeB2.x2App[0].client.connectAddress = "eNodeB1%x2ppp0"
*.eNodeB2.x2App[1].client.connectAddress = "eNodeB3%x2ppp1"
*.eNodeB2.x2App[2].client.connectAddress = "eNodeB4%x2ppp1"

*.eNodeB3.x2App[0].client.connectAddress = "eNodeB1%x2ppp1"
*.eNodeB3.x2App[1].client.connectAddress = "eNodeB2%x2ppp1"
*.eNodeB3.x2App[2].client.connectAddress = "eNodeB4%x2ppp2"

*.eNodeB4.x2App[0].client.connectAddress = "eNodeB1%x2ppp2"
*.eNodeB4.x2App[1].client.connectAddress = "eNodeB2%x2ppp2"
*.eNodeB4.x2App[2].client.connectAddress = "eNodeB3%x2ppp2"

**.sctp.nagleEnabled = false
**.sctp.enableHeartbeats = false

##########################################################
#                    App Layer                           #
##########################################################
[Config VoIP-UL]
*.server.numApps = ${numUEs}
*.server.app[*].typename = "VoIPReceiver"
*.server.app[*].localPort = 3000 + ancestorIndex(0)

*.car[*].numApps = 1
*.car[*].app[0].typename = "VoIPSender"
*.car[*].app[0].destAddress = "server"
*.car[*].app[0].destPort = 3000 + ancestorIndex(1)

[Config VoIP-DL]
*.server.numApps = ${numUEs}
*.server.app[*].typename = "VoIPSender"
*.server.app[*].localPort = 3000 + ancestorIndex(0)
*.server.app[*].destAddress = "car[" + string(ancestorIndex(0)) + "]"
*.server.app[*].startingTime = 0.05s

*.car[*].numApps = 1
*.car[*].app[0].typename = "VoIPReceiver"

# ════════════════════════════════════════════════════════════
#  TrafficApp — V2N Trafik Yonlendirme
# ════════════════════════════════════════════════════════════
[Config TrafficApp]

*.car[*].numApps = 1
*.car[*].app[0].typename = "VehicleApp"
*.car[*].app[0].reportInterval = 3s
*.car[*].app[0].serverAddress = "server"
*.car[*].app[0].serverPort = 7000
*.car[*].app[0].localPort = 6000 + ancestorIndex(1)
*.car[*].app[0].netXmlFile = "most.net.xml"
*.car[*].app[0].penetrationRate = 0.0

*.server.app[0].rerouteInterval = 60s

*.server.numApps = 1
*.server.app[0].typename = "TrafficServer"
*.server.app[0].listenPort = 7000
*.server.app[0].vehicleBasePort = 6000
*.server.app[0].speedLimit = 13.9mps
*.server.app[0].decayInterval = 5s
*.server.app[0].netXmlFile = "most.net.xml"
*.server.app[0].minRouteDistance = 800m

**.crcMode = "computed"
**.cellInfo.numRbDl = 100
**.cellInfo.numRbUl = 100
**.numBands = 100
**.maxHarqRtx = 2

# ════════════════════════════════════════════════════════════
#  Penetrasyon Senaryolari
# ════════════════════════════════════════════════════════════
[Config Baseline]
extends = TrafficApp
description = "Referans — tum araclar SUMO varsayilan rotasiyla gider"
*.car[*].app[0].penetrationRate = 0.0
seed-set = 2

[Config Penetration15]
extends = TrafficApp
description = "Araclarin %15'i sunucudan dinamik rota alir"
*.car[*].app[0].penetrationRate = 0.15
*.server.app[0].K = 1.5
seed-set = 2

[Config Penetration30]
extends = TrafficApp
description = "Araclarin %30'u sunucudan dinamik rota alir"
*.car[*].app[0].penetrationRate = 0.30
*.server.app[0].K = 1.5
seed-set = 2

[Config Penetration45]
extends = TrafficApp
description = "Araclarin %45'i sunucudan dinamik rota alir"
*.car[*].app[0].penetrationRate = 0.45
*.server.app[0].K = 1.5
seed-set = 2

[Config Penetration60]
extends = TrafficApp
description = "Araclarin %60'u sunucudan dinamik rota alir"
*.car[*].app[0].penetrationRate = 0.60
*.server.app[0].K = 1.5
seed-set = 2

[Config Penetration75]
extends = TrafficApp
description = "Araclarin %75'i sunucudan dinamik rota alir"
*.car[*].app[0].penetrationRate = 0.75
*.server.app[0].K = 1.5
seed-set = 2

[Config TestCrash]
extends = TrafficApp
description = "Crash testi - tum araclar LTE"
*.car[*].app[0].penetrationRate = 1.0

[Config Penetration40k1_5]
extends = TrafficApp
description = "Araclarin %40'u K=1.5"
*.car[*].app[0].penetrationRate = 0.40
*.server.app[0].K = 1.5
seed-set = 1

[Config Penetration40k2]
extends = TrafficApp
description = "Araclarin %40'u K=2"
*.car[*].app[0].penetrationRate = 0.40
*.server.app[0].K = 2
seed-set = 1
EOF

echo "    omnetpp.ini olusturuldu."

# ════════════════════════════════════════════════
#  ADIM 7: Highway.ned
# ════════════════════════════════════════════════
echo ""
echo "[7/8] Highway.ned olusturuluyor..."

cat > Highway.ned << 'EOF'
package lte.simulations.cars;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import inet.node.ethernet.Eth10G;
import lte.world.radio.LteChannelControl;
import lte.epc.PgwStandardSimplified;
import lte.corenetwork.binder.LteBinder;
import lte.corenetwork.nodes.eNodeB;
import lte.corenetwork.nodes.cars.Car;
import lte.common.LteNetworkConfigurator;
import org.car2x.veins.subprojects.veins_inet.VeinsInetManager;
import inet.environment.common.PhysicalEnvironment;
import org.car2x.veins.visualizer.roads.RoadsCanvasVisualizer;
network Highway
{
    parameters:
        double playgroundSizeX @unit(m);
        double playgroundSizeY @unit(m);
        double playgroundSizeZ @unit(m);
        @display("bgb=10100,6500");
    submodules:
        routingRecorder: RoutingTableRecorder {
            @display("p=50,75;is=s");
        }
        configurator: LteNetworkConfigurator {
            @display("p=50,125");
        }
        veinsManager: VeinsInetManager {
            @display("p=50,227;is=s");
        }
        channelControl: LteChannelControl {
            @display("p=50,25;is=s");
        }
        binder: LteBinder {
            @display("p=50,175;is=s");
        }
        physicalEnvironment: PhysicalEnvironment {
            @display("p=715,69");
        }
        roadsCanvasVisualizer: RoadsCanvasVisualizer {
            @display("p=1424,69");
        }
        server: StandardHost {
            @display("p=8500,3200;is=n;i=device/server");
        }
        router: Router {
            @display("p=8500,1500;i=device/smallrouter");
        }
        pgw: PgwStandardSimplified {
            nodeType = "PGW";
            @display("p=6500,1500;is=l");
        }
        eNodeB1: eNodeB {
            @display("p=2500,1600;is=l");
        }
        eNodeB2: eNodeB {
            @display("p=7500,1600;is=l");
        }
        eNodeB3: eNodeB {
            @display("p=2500,4800;is=l");
        }
        eNodeB4: eNodeB {
            @display("p=7500,4800;is=l");
        }
    connections allowunconnected:
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> pgw.filterGate;
        pgw.pppg++ <--> Eth10G <--> eNodeB1.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB2.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB3.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB4.ppp;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB2.x2++;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB3.x2++;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB4.x2++;
        eNodeB2.x2++ <--> Eth10G <--> eNodeB3.x2++;
        eNodeB2.x2++ <--> Eth10G <--> eNodeB4.x2++;
        eNodeB3.x2++ <--> Eth10G <--> eNodeB4.x2++;
}
EOF

echo "    Highway.ned olusturuldu."

# ════════════════════════════════════════════════
#  ADIM 8: Derle + Calistir
# ════════════════════════════════════════════════
echo ""
echo "[8/8] Proje derleniyor ve $SCENARIO baslatiliyor..."

cd "$HOME/my_workspace/test_workspace/simulte/src"
make -j$(nproc) 2>&1 | tail -5
echo "    Derleme tamamlandi."

cd "$PROJECT_DIR"
mkdir -p results

# tripinfo ayarla
sed -i '/<output>/,/<\/output>/d' most.sumo.cfg
sed -i '/<\/time>/a\    <output>\n        <tripinfo-output value="/home/veins/my_workspace/test_workspace/simulte/simulations/cars/results/tripinfo_'"$SCENARIO"'.xml"/>\n    </output>' most.sumo.cfg

pkill -f sumo 2>/dev/null
sleep 2

echo ""
echo "============================================"
echo "  $SCENARIO baslatiliyor — $(date)"
echo "============================================"

OPP_RUN="opp_run \
-l /home/veins/my_workspace/test_workspace/simulte/src/lte \
-l /home/veins/my_workspace/test_workspace/veins_inet/src/veins_inet \
-l /home/veins/my_workspace/test_workspace/veins/src/veins \
-l /home/veins/my_workspace/test_workspace/inet/src/INET \
-n /home/veins/my_workspace/test_workspace/simulte/src:/home/veins/my_workspace/test_workspace/simulte/simulations:/home/veins/my_workspace/test_workspace/inet/src:/home/veins/my_workspace/test_workspace/veins_inet/src/veins_inet:/home/veins/my_workspace/test_workspace/veins/src/veins:/home/veins/my_workspace/test_workspace/simulte/simulations/cars \
-u Cmdenv"

$OPP_RUN -c "$SCENARIO" omnetpp.ini 2>&1 | tee "results/${SCENARIO}.log"

echo ""
echo "============================================"
echo "  $SCENARIO TAMAMLANDI — $(date)"
echo "============================================"
COUNT=$(grep -c "<tripinfo " "results/tripinfo_${SCENARIO}.xml" 2>/dev/null || echo "0")
echo "  Tamamlanan arac: $COUNT"
echo "============================================"
