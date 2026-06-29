package lte.simulations.cars;
import inet.applications.contract.IApp;

// ============================================================
//  VehicleApp.ned — araç uygulama modülü (EV güncellemesi)
//
//  isElectric: SUMO'dan otomatik tespit (evType kontrol)
//  socThreshold: şarj ihtiyacı eşiği (%)
// ============================================================
simple VehicleApp like IApp
{
    parameters:
        double reportInterval @unit(s) = default(1s);
        string serverAddress = default("server");
        int    serverPort    = default(7000);
        int    localPort     = default(6000);
        string netXmlFile = default("istanbul-v1.net.xml");

        // EV parametresi
        double socThreshold = default(20);   // şarj uyarı eşiği (%)

		double penetrationRate = default(0.0);
        @class(VehicleApp);
        @display("i=block/app");

    gates:
        input  socketIn;
        output socketOut;
}
