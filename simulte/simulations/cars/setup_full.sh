#!/bin/bash
# ================== TEK DEGISKEN ==================
SCENARIO_LIST='Baseline Penetration15'   # bu sunucuda kosacak senaryolar (boslukla ayir)
# Ornekler: 'Penetration30'  |  'Penetration45'  |  'Penetration60'  |  'Baseline Penetration15'
# =================================================

SIM=/home/veins/my_workspace/test_workspace/simulte
SIMDIR=$SIM/simulations/cars
SRC=$SIM/src

echo "===== LuST 9-Node Full Setup — $(date) ====="

# --- 0) Calisan simulasyonu durdur ---
pkill -9 -f opp_run 2>/dev/null; pkill -9 -f sumo 2>/dev/null; sleep 3
echo "[0] Calisan simulasyonlar durduruldu"

# --- 1) DasFilter.cc null check ---
python3 - << 'PYEOF'
f='/home/veins/my_workspace/test_workspace/simulte/src/stack/phy/das/DasFilter.cc'
c=open(f).read()
if 'module == nullptr' in c:
    print("[1] DasFilter fix zaten var")
else:
    old='    cModule* module = getSimulation()->getModule(binder_->getOmnetId(masterId));\n    if (getNodeTypeById(masterId) == ENODEB)'
    new='''    cModule* module = getSimulation()->getModule(binder_->getOmnetId(masterId));
    if (module == nullptr)
    {
        ruSet_ = nullptr;
        reportingSet_.clear();
        return;
    }
    if (getNodeTypeById(masterId) == ENODEB)'''
    if old in c:
        open(f,'w').write(c.replace(old,new)); print("[1] DasFilter fix uygulandi")
    else:
        print("[1] HATA: DasFilter anchor yok")
PYEOF

# --- 2) LtePhyUe.cc handover null check ---
python3 - << 'PYEOF'
f='/home/veins/my_workspace/test_workspace/simulte/src/stack/phy/layer/LtePhyUe.cc'
c=open(f).read()
if 'masterMod == nullptr' in c:
    print("[2] LtePhyUe fix zaten var")
else:
    old='    IP2lte* enbIp2lte =  check_and_cast<IP2lte*>(getSimulation()->getModule(binder_->getOmnetId(masterId_))->getSubmodule("lteNic")->getSubmodule("ip2lte"));'
    new='''    cModule* masterMod = getSimulation()->getModule(binder_->getOmnetId(masterId_));
    if (masterMod == nullptr)
    {
        binder_->removeUeHandoverTriggered(nodeId_);
        return;
    }
    IP2lte* enbIp2lte =  check_and_cast<IP2lte*>(masterMod->getSubmodule("lteNic")->getSubmodule("ip2lte"));'''
    if old in c:
        open(f,'w').write(c.replace(old,new)); print("[2] LtePhyUe fix uygulandi")
    else:
        print("[2] HATA: LtePhyUe anchor yok")
PYEOF

# --- 3) Highway.ned: 9 eNodeB + X2 ---
cat > $SIMDIR/Highway.ned << 'NEDEOF'
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
        @display("bgb=13700,11600");
    submodules:
        routingRecorder: RoutingTableRecorder { @display("p=50,75;is=s"); }
        configurator: LteNetworkConfigurator { @display("p=50,125"); }
        veinsManager: VeinsInetManager { @display("p=50,227;is=s"); }
        channelControl: LteChannelControl { @display("p=50,25;is=s"); }
        binder: LteBinder { @display("p=50,175;is=s"); }
        physicalEnvironment: PhysicalEnvironment { @display("p=715,69"); }
        roadsCanvasVisualizer: RoadsCanvasVisualizer { @display("p=1424,69"); }
        server: StandardHost { @display("p=12500,5800;is=n;i=device/server"); }
        router: Router { @display("p=12500,3000;i=device/smallrouter"); }
        pgw: PgwStandardSimplified {
            nodeType = "PGW";
            @display("p=10000,3000;is=l");
        }
        eNodeB1: eNodeB { @display("p=6500,6000;is=l"); }
        eNodeB2: eNodeB { @display("p=7500,5500;is=l"); }
        eNodeB3: eNodeB { @display("p=6000,7500;is=l"); }
        eNodeB4: eNodeB { @display("p=8000,6500;is=l"); }
        eNodeB5: eNodeB { @display("p=8500,3500;is=l"); }
        eNodeB6: eNodeB { @display("p=4000,6800;is=l"); }
        eNodeB7: eNodeB { @display("p=3000,7500;is=l"); }
        eNodeB8: eNodeB { @display("p=7500,8500;is=l"); }
        eNodeB9: eNodeB { @display("p=10000,3000;is=l"); }
    connections allowunconnected:
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> pgw.filterGate;
        pgw.pppg++ <--> Eth10G <--> eNodeB1.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB2.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB3.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB4.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB5.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB6.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB7.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB8.ppp;
        pgw.pppg++ <--> Eth10G <--> eNodeB9.ppp;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB2.x2++;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB3.x2++;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB4.x2++;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB5.x2++;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB6.x2++;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB8.x2++;
        eNodeB2.x2++ <--> Eth10G <--> eNodeB3.x2++;
        eNodeB2.x2++ <--> Eth10G <--> eNodeB4.x2++;
        eNodeB2.x2++ <--> Eth10G <--> eNodeB5.x2++;
        eNodeB2.x2++ <--> Eth10G <--> eNodeB8.x2++;
        eNodeB3.x2++ <--> Eth10G <--> eNodeB4.x2++;
        eNodeB3.x2++ <--> Eth10G <--> eNodeB6.x2++;
        eNodeB3.x2++ <--> Eth10G <--> eNodeB7.x2++;
        eNodeB3.x2++ <--> Eth10G <--> eNodeB8.x2++;
        eNodeB4.x2++ <--> Eth10G <--> eNodeB5.x2++;
        eNodeB4.x2++ <--> Eth10G <--> eNodeB8.x2++;
        eNodeB5.x2++ <--> Eth10G <--> eNodeB9.x2++;
        eNodeB6.x2++ <--> Eth10G <--> eNodeB7.x2++;
}
NEDEOF
echo "[3] Highway.ned (9 eNodeB + X2) yazildi"

# --- 4) omnetpp.ini: 9 node + handover + dynamicCell + X2 + reportInterval ---
python3 - << 'PYEOF'
import re
f='/home/veins/my_workspace/test_workspace/simulte/simulations/cars/omnetpp.ini'
lines=open(f).read().split('\n')
out=[]; n=len(lines); i=0
enb=[(6500,6000),(7500,5500),(6000,7500),(8000,6500),(8500,3500),(4000,6800),(3000,7500),(7500,8500),(10000,3000)]
while i<n:
    l=lines[i]
    if l.startswith('*.eNodeB1.mobility.initialX'):
        for k,(x,y) in enumerate(enb,1):
            out+=[f'*.eNodeB{k}.mobility.initialX = {x}m',f'*.eNodeB{k}.mobility.initialY = {y}m',f'*.eNodeB{k}.mobility.initialZ = 30m']
        while i<n and '.mobility.initial' in lines[i]: i+=1
        continue
    if 'dynamicCellAssociation' in l:
        out.append('*.car[*].lteNic.phy.dynamicCellAssociation = true'); i+=1; continue
    if l.startswith('**.eNodeB1.macCellId'):
        for k in range(1,10):
            out+=[f'**.eNodeB{k}.macCellId = {k}',f'**.eNodeB{k}.macNodeId = {k}']
        out.append('**.eNodeBCount = 9')
        while i<n and ('.macCellId' in lines[i] or '.macNodeId' in lines[i] or 'eNodeBCount' in lines[i]): i+=1
        continue
    if 'car[*].lteNic.phy.enableHandover' in l:
        out.append('*.car[*].lteNic.phy.enableHandover = true'); i+=1; continue
    if 'eNodeB*.lteNic.phy.enableHandover' in l:
        out.append('*.eNodeB*.lteNic.phy.enableHandover = true'); i+=1; continue
    if 'numX2Apps' in l or '.x2App[' in l:
        i+=1; continue
    if 'reportInterval' in l:
        out.append('*.car[*].app[0].reportInterval = 10s'); i+=1; continue
    out.append(l); i+=1
c='\n'.join(out)
# X2 blogunu broadcastMessageInterval'den sonra ekle
x2lines=['*.eNodeB*.x2App[*].server.localPort = 5000 + ancestorIndex(1)']
x2lines+=[f'*.eNodeB{k}.numX2Apps = {v}' for k,v in zip(range(1,10),[6,5,6,5,4,3,2,4,1])]
x2map=[(1,0,'eNodeB2',0),(1,1,'eNodeB3',0),(1,2,'eNodeB4',0),(1,3,'eNodeB5',0),(1,4,'eNodeB6',0),(1,5,'eNodeB8',0),
(2,0,'eNodeB1',0),(2,1,'eNodeB3',1),(2,2,'eNodeB4',1),(2,3,'eNodeB5',1),(2,4,'eNodeB8',1),
(3,0,'eNodeB1',1),(3,1,'eNodeB2',1),(3,2,'eNodeB4',2),(3,3,'eNodeB6',1),(3,4,'eNodeB7',0),(3,5,'eNodeB8',2),
(4,0,'eNodeB1',2),(4,1,'eNodeB2',2),(4,2,'eNodeB3',2),(4,3,'eNodeB5',2),(4,4,'eNodeB8',3),
(5,0,'eNodeB1',3),(5,1,'eNodeB2',3),(5,2,'eNodeB4',3),(5,3,'eNodeB9',0),
(6,0,'eNodeB1',4),(6,1,'eNodeB3',3),(6,2,'eNodeB7',1),
(7,0,'eNodeB3',4),(7,1,'eNodeB6',2),
(8,0,'eNodeB1',5),(8,1,'eNodeB2',4),(8,2,'eNodeB3',5),(8,3,'eNodeB4',4),
(9,0,'eNodeB5',3)]
for e,k,tgt,ppp in x2map:
    x2lines.append(f'*.eNodeB{e}.x2App[{k}].client.connectAddress = "{tgt}%x2ppp{ppp}"')
x2block='\n'.join(x2lines)
c=c.replace('*.eNodeB*.lteNic.phy.broadcastMessageInterval = 2s','*.eNodeB*.lteNic.phy.broadcastMessageInterval = 2s\n'+x2block)
open(f,'w').write(c)
print("[4] omnetpp.ini guncellendi (9 node + handover + X2 + reportInterval 10s)")
PYEOF

# --- 5) run_lust.sh ---
cat > $SIMDIR/run_lust.sh << RUNEOF
#!/bin/bash
SCENARIOS=($SCENARIO_LIST)
SIMDIR=/home/veins/my_workspace/test_workspace/simulte/simulations/cars
OPP="opp_run -l $SIM/src/lte -l $SIM/../veins_inet/src/veins_inet -l $SIM/../veins/src/veins -l $SIM/../inet/src/INET -n $SIM/src:$SIM/simulations:$SIM/../inet/src:$SIM/../veins_inet/src/veins_inet:$SIM/../veins/src/veins:$SIMDIR -u Cmdenv"
cd \$SIMDIR || exit 1
mkdir -p results
for S in "\${SCENARIOS[@]}"; do
  echo ">>> \$S basliyor — \$(date)"
  pkill -f sumo 2>/dev/null; sleep 2
  sed -i "s|<tripinfo-output value=\"[^\"]*\"/>|<tripinfo-output value=\"\$SIMDIR/results/tripinfo_\${S}.xml\"/>|" lust.sumo.cfg
  \$OPP -c "\$S" omnetpp.ini 2>&1 | tee "results/\${S}.log"
  echo ">>> \$S tamamlandi — \$(date)"
done
echo "Tum senaryolar bitti — \$(date)"
RUNEOF
chmod +x $SIMDIR/run_lust.sh
echo "[5] run_lust.sh yazildi (senaryo: $SCENARIO_LIST)"

# --- 6) results temizle ---
mkdir -p $SIMDIR/results/olds_$(date +%Y%m%d_%H%M)
mv $SIMDIR/results/*.log $SIMDIR/results/*.sca $SIMDIR/results/*.xml $SIMDIR/results/*.csv $SIMDIR/results/olds_$(date +%Y%m%d_%H%M)/ 2>/dev/null
echo "[6] results temizlendi"

# --- 7) Derle ---
echo "[7] Derleme basliyor (birkac dakika)..."
cd $SIM && make -j4 2>&1 | tail -3

echo ""
echo "===== SETUP TAMAM — $(date) ====="
echo "Test icin:  cd $SIMDIR && timeout 90 bash run_lust.sh"
echo "Tam calistir: cd $SIMDIR && nohup bash run_lust.sh > results/master.log 2>&1 &"
