package lte.simulations.cars;

import inet.applications.contract.IApp;

simple VehicleReRouteApp like IApp
{
    parameters:
        double checkInterval @unit(s) = default(1s);
        string targetEdgeId = default("5170334#0");
        string watchEdgeId = default("826449813");
        double artificialTravelTime @unit(s) = default(3600s);
        @class(VehicleReRouteApp);
        @display("i=block/app");
    gates:
        input socketIn;
        output socketOut;
}
