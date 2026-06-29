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
