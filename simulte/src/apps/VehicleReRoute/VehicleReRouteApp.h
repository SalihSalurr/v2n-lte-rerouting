#pragma once
#include <omnetpp.h>
#include "inet/common/INETDefs.h"
#include "veins_inet/VeinsInetMobility.h"
#include "veins/modules/mobility/traci/TraCICommandInterface.h"

using namespace omnetpp;
using namespace veins;

//#define REROUTE_MODE_NEW_ROUTE
//#define REROUTE_MODE_CHANGE_ROUTE
//#define REROUTE_MODE_CHANGE_VEHICLE
//#define REROUTE_MODE_CHANGE_TARGET
#define REROUTE_MODE_GET_PLANNED

class VehicleReRouteApp : public cSimpleModule
{
  protected:
    veins::VeinsInetMobility* mobility = nullptr;
    cMessage* timerCheck = nullptr;
    bool rerouteDone = false;

    double      checkInterval;
    std::string targetEdgeId;
    std::string watchEdgeId;
    simtime_t   artificialTravelTime;

    void log(const std::string& msg);

    virtual void initialize(int stage) override;
    virtual int  numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage* msg) override;
    virtual void finish() override;

    void doReroute_newRoute();
    void doReroute_changeRoute();
    void doReroute_changeVehicleRoute();
    void doReroute_changeTarget();
    void doReroute_getPlannedOnly();

    veins::TraCICommandInterface::Vehicle getVehicle();
};
