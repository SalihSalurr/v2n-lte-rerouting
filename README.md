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
- **`simulte/src/stack/mac/scheduler/LteSchedulerEnb.cc`** — `allocatedCws_.at()` guarded with `find()` check, prevents `std::out_of_range` crash under high-load scheduling (observed at higher penetration rates, ~40k vehicles)
- **`simulte/src/stack/mac/layer/LteMacEnb.cc`** — `getCellInfo()` now checks submodule existence before cast, returns `nullptr` instead of crashing when `cellInfo` submodule isn't found (observed during handover under high UE density)
- **`simulte/src/corenetwork/lteip/IP2lte.cc`** — `hoManager_` lazy-init in three handover functions now checks submodule existence before `check_and_cast`, prevents crash when `handoverManager` submodule isn't ready yet

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

### Additional AMC/Scheduler Crash Fixes (found via GDB backtrace analysis)

Discovered while stress-testing Pen30/45/60 scenarios under GDB (`catch throw`) after initial fixes proved insufficient. Root cause: `LteAmc` node-index maps (`dlNodeIndex_`, `ulNodeIndex_`, `d2dNodeIndex_`) and lookup tables can be accessed with a `nodeId`/CQI/MCS value before that UE is fully registered, especially under high penetration rate with many simultaneous LTE attachments.

- **`simulte/src/stack/mac/amc/LteAmc.cc`** — `existTxParams()`, `getFeedback()`, `setTxParams()` now check node-index map membership via `find()` before `.at()` access; return safe defaults (`false`, empty feedback, unchanged `info`) instead of throwing `std::out_of_range`
- **`simulte/src/stack/mac/amc/LteAmc.cc`** — `getItbsPerCqi()` clamps out-of-range `cqi` values to the valid table bound (0–15) before indexing `cqiTable[]`, preventing a segfault from unchecked array access
- **`simulte/src/stack/mac/amc/LteMcs.cc`** — `itbs2tbs()` falls back to `_QPSK` when passed an invalid `LteMod` value instead of throwing `cRuntimeError`, and returns `nullptr` safely from the (now unreachable but retained) default branches

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

## Author

Salih Salur — [GitHub](https://github.com/SalihSalurr)

### Vehicle Lifecycle / Stale MacNodeId Crash Fixes (in progress)

SimuLTE + Veins crashes when a vehicle module is deleted mid-simulation
(vehicle leaves the SUMO map). The LTE stack scatters `MacNodeId`
references across AMC, schedulers, and the binder, and cleanup on
deletion is incomplete — leading to `SIGSEGV` in
`LteAmc::computeBitsOnNRbs` or `std::out_of_range` in `LteAmc::detachUser`.

**Workaround applied (6 files):**

- **`veins/src/veins/modules/mobility/traci/TraCIScenarioManager.cc`** —
  `deleteManagedModule()` no longer calls `callFinish()` / `deleteModule()`.
  Vehicle modules are disconnected from the channel but kept alive in
  OMNeT++, so the LTE stack never receives a "vehicle gone" event. Trades
  memory for stability (module count grows with total vehicle churn).
- **`simulte/src/corenetwork/binder/LteBinder.cc`** — `unregisterNode()`
  now proactively detaches the UE from every eNB's AMC (all directions)
  and clears scheduler queues, as a safety net independent of teardown order.
- **`simulte/src/stack/mac/amc/LteAmc.cc`** — `detachUser()` guards
  against UEs never attached in a given direction; `computeTxParams()`
  returns a static invalid sentinel for stale/unregistered nodeIds, checked
  via `isSet()` at all 5 downstream call sites (`computeReqRbs`,
  `computeBitsOnNRbs` x2, `computeBitsOnNRbs_MB`, `readCoderate`).
- **`simulte/src/stack/mac/amc/UserTxParams.h`** — added `const isSet()`
  overload; `getCwModulation()` / `getCwRate()` bounds-check the CQI value
  against `MAXCQI` before indexing `cqiTable[]`.
- **`simulte/src/stack/mac/scheduler/LteSchedulerEnb.cc`** —
  `scheduleGrant()` checks binder liveness for the target nodeId before
  proceeding, acting as a single upstream chokepoint.
- **`simulte/src/apps/TrafficApp/VehicleApp.cc`** — `sendReport()` probes
  TraCI liveness (`getRoadId()`) and permanently disables itself
  (`isEquipped = false`) once the vehicle is confirmed gone from SUMO.

**Status:** Penetration30 reached t=937s (~12% of a 3.088e6s / ~858h
scenario timeline — need to confirm actual `sim-time-limit`) with RAM
usage ~2.9GB / 25GB free, before hitting an unrelated handover crash:
This is a separate bug: during handover, `LtePhyUe::doHandover()` calls
`getAmcModule()` which does `check_and_cast<LteMacEnb*>` on what may be
a stale/incorrect `masterId_`, landing on a UE's MAC module instead of
an eNB's. Root cause and fix are pending follow-up work.
