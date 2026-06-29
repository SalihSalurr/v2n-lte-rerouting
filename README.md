# V2N LTE-Based Traffic Rerouting Simulation



> **⚠️ Active Research:** This project is ongoing. The codebase, configurations, and results are subject to change.



Investigating the impact of **LTE penetration rate** on Vehicle-to-Network (V2N) traffic rerouting effectiveness using the Luxembourg SUMO Traffic (LuST) scenario during peak-hour congestion.



## Overview



This project simulates a realistic urban traffic scenario where a central server collects real-time traffic data from LTE-connected vehicles and issues rerouting commands to alleviate congestion. The key research question: **How does the percentage of LTE-equipped vehicles affect rerouting performance?**



### Simulation Stack



```

┌─────────────┐     TraCI      ┌─────────────┐

│    SUMO      │◄──────────────►│   Veins      │

│  (Traffic)   │                │  (V2X Comm)  │

└─────────────┘                └──────┬───────┘

                                      │

                               ┌──────┴───────┐

                               │   SimuLTE     │

                               │  (LTE Stack)  │

                               └──────┬───────┘

                                      │

                               ┌──────┴───────┐

                               │    INET       │

                               │  (Network)    │

                               └──────────────┘

```



## Environment



This project runs on the [Veins virtual machine (Debian-based)](https://veins.car2x.org), which provides a pre-configured environment with OMNeT++, SUMO, and Veins. SimuLTE and INET are added on top.



## Modified Files



### SimuLTE Crash Fixes (Null Pointer Checks)



SimuLTE lacks null checks when Veins dynamically adds vehicles at high density. Modules (cellInfo, binder, eNB MAC) may not be ready when a vehicle spawns, causing segfaults.



- **`simulte/src/stack/phy/layer/LtePhyUe.cc`** — `cellInfo_` null check in `initialize()`, prevents crash when eNB not yet assigned

- **`simulte/src/stack/phy/das/DasFilter.cc`** — `module` null check in `setMasterRuSet()`, prevents crash during handover with dynamicCellAssociation

- **`simulte/src/corenetwork/lteCellInfo/LteCellInfo.h`** — `binder_` null check in `lambdaUpdate()`, prevents crash when binder not initialized

- **`simulte/src/corenetwork/lteCellInfo/LteCellInfo.cc`** — `binder_` null check in `attachUser()`, prevents crash during cell registration

- **`simulte/src/stack/mac/layer/LteMacUe.cc`** — eNB module chain null check, breaks chained `getSubmodule()` calls into safe steps



### Application Layer (Created)



- **`simulte/src/apps/TrafficApp/TrafficServer.cc/.h`** — Central rerouting server: receives vehicle reports, computes congestion, runs Dijkstra, sends reroute commands

- **`simulte/src/apps/TrafficApp/VehicleApp.cc/.h`** — Vehicle application: reports edge ID, speed, travel time to server via UDP/LTE, applies reroute commands

- **`simulte/src/apps/TrafficApp/TrafficMsg.msg`** — Message definitions (VehicleReportMsg, ServerRouteMsg)

- **`simulte/src/apps/TrafficApp/TrafficMsgSerializer.cc/.h`** — Custom serializer for LTE stack compatibility



### Network Topology & Configuration



- **`simulte/simulations/cars/Highway.ned`** — 9 eNodeB topology with full X2 mesh for LuST map coverage

- **`simulte/simulations/cars/omnetpp.ini`** — Simulation configuration: eNodeB coordinates, handover, dynamicCellAssociation, penetration rates

- **`simulte/simulations/cars/lust_peak.rou.xml`** — LuST peak-hour vehicle routes (07:00–09:00, ~40k vehicles), derived from the [LuST Scenario](https://github.com/lcodeca/LuSTScenario) by Codecà et al.

- **`simulte/simulations/cars/setup_full.sh`** — One-command deployment script



## Prerequisites



- [Veins VM](https://veins.car2x.org) (Debian-based, includes OMNeT++ 5.x, SUMO, and Veins 5.x)

- INET 3.x

- SimuLTE



## References



- L. Codecà, R. Frank, S. Faye and T. Engel, "Luxembourg SUMO Traffic (LuST) Scenario: Traffic Demand Evaluation," *IEEE Intelligent Transportation Systems Magazine*, vol. 9, no. 2, pp. 52-63, Summer 2017. [GitHub](https://github.com/lcodeca/LuSTScenario)

- C. Sommer, R. German, and F. Dressler, "Bidirectionally Coupled Network and Road Traffic Simulation for Improved IVC Analysis," *IEEE Transactions on Mobile Computing*, vol. 10, no. 1, pp. 3-15, January 2011. [Veins](https://veins.car2x.org)

- A. Virdis, G. Stea, and G. Nardini, "SimuLTE - A Modular System-level Simulator for LTE/LTE-A Networks based on OMNeT++," *International Conference on Simulation and Modeling Methodologies, Technologies and Applications (SIMULTECH)*, 2014. [SimuLTE](https://simulte.com)

- A. Varga and R. Hornig, "An Overview of the OMNeT++ Simulation Environment," *International ICST Conference on Simulation Tools and Techniques (SIMUWorks)*, 2008. [OMNeT++](https://omnetpp.org)

- P. A. Lopez et al., "Microscopic Traffic Simulation using SUMO," *IEEE Intelligent Transportation Systems Conference (ITSC)*, 2018. [SUMO](https://sumo.dlr.de)



## License



This repository contains modified versions of open-source simulation frameworks. Each framework retains its original license. Research modifications are provided for academic purposes.



## Author



Salih Salur — [GitHub](https://github.com/SalihSalurr)

