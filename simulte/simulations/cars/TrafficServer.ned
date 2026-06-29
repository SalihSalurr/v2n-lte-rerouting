package lte.simulations.cars;
import inet.applications.contract.IApp;

// ============================================================
//  TrafficServer.ned — merkezi trafik sunucusu (EV güncellemesi)
// ============================================================
simple TrafficServer like IApp
{
    parameters:
        int listenPort = default(5000);
        int vehicleBasePort = default(6000);
        double K = default(3.0);
        double speedLimit @unit(mps) = default(13.9mps);
        double normalTravelTime @unit(s) = default(30s);
        double decayInterval @unit(s) = default(5s);
        string netXmlFile = default("istanbul-v1.net.xml");

        // EV parametreleri (YENİ)
        double socThreshold = default(20);            // şarj eşiği (%)
        double chargingTimePerKwh = default(180);     // kWh başına şarj süresi (s)
        double decayHalfLife = default(900);           // tıkanıklık decay yarı ömrü (s)
        string additionalFile = default("");           // SUMO additional.xml

		double rerouteInterval @unit(s) = default(30s);
		double minRouteDistance @unit(m) = default(1500m);
        @class(TrafficServer);
        @display("i=block/server");

    gates:
        input  socketIn;
        output socketOut;
}
