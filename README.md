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

Originally developed on the [Veins virtual machine (Debian-based)](https://veins.car2x.org), which provides a pre-configured environment with OMNeT++, SUMO, and Veins. SimuLTE and INET are added on top.

Long simulation runs are executed on an Azure VM (Ubuntu 24.04, 4 vCPU, 32 GB RAM) with OMNeT++ 5.7, INET 4, Veins, `veins_inet`, and SimuLTE built from source.

## Prerequisites

- OMNeT++ 5.7
- INET 4.x
- Veins 5.x + `veins_inet`
- SimuLTE
- SUMO 1.8.0

---

## Modified Files

### Application Layer (Created)

- **`simulte/src/apps/TrafficApp/TrafficServer.cc/.h`** — Central rerouting server: receives vehicle reports, computes congestion, runs Dijkstra, sends reroute commands
- **`simulte/src/apps/TrafficApp/VehicleApp.cc/.h`** — Vehicle application: reports edge ID, speed, travel time to server via UDP/LTE, applies reroute commands, and writes per-vehicle CSV statistics
- **`simulte/src/apps/TrafficApp/TrafficMsg.msg`** — Message definitions (VehicleReportMsg, ServerRouteMsg)
- **`simulte/src/apps/TrafficApp/TrafficMsgSerializer.cc/.h`** — Custom serializer for LTE stack compatibility
- **`simulte/src/apps/VehicleReRoute/VehicleReRouteApp.cc/.h`** — Per-vehicle rerouting client (edge check loop, one reroute per vehicle)

### Network Topology & Configuration

- **`simulte/simulations/cars/Highway.ned`** — 9 eNodeB topology with full X2 mesh for LuST map coverage
- **`simulte/simulations/cars/omnetpp.ini`** — Simulation configuration: eNodeB coordinates, handover, dynamicCellAssociation, penetration rates
- **`simulte/simulations/cars/lust_peak.rou.xml`** — LuST peak-hour vehicle routes (~20.6k completed trips over a 2 h window), derived from the [LuST Scenario](https://github.com/lcodeca/LuSTScenario) by Codecà et al.
- **`simulte/simulations/cars/lust.sumo.cfg`** — SUMO config; `tripinfo-output` is renamed per scenario before each run
- **`simulte/simulations/cars/setup_full.sh`** — One-command deployment script

---

## SimuLTE Crash Fixes

SimuLTE was not designed for the vehicle churn that Veins produces: vehicles are added and removed continuously, and `MacNodeId` references are scattered across the AMC, schedulers, binder, PHY, and DAS filter. The fixes below fall into three waves.

### Wave 1 — Startup / Null-Pointer Fixes

SimuLTE lacks null checks when Veins dynamically adds vehicles at high density. Modules (cellInfo, binder, eNB MAC) may not be ready when a vehicle spawns, causing segfaults.

- **`simulte/src/stack/phy/layer/LtePhyUe.cc`** — `cellInfo_` null check in `initialize()`, prevents crash when eNB not yet assigned
- **`simulte/src/stack/phy/das/DasFilter.cc`** — `module` null check in `setMasterRuSet()`, prevents crash during handover with dynamicCellAssociation
- **`simulte/src/corenetwork/lteCellInfo/LteCellInfo.h`** — `binder_` null check in `lambdaUpdate()`
- **`simulte/src/corenetwork/lteCellInfo/LteCellInfo.cc`** — `binder_` null check in `attachUser()`
- **`simulte/src/stack/mac/layer/LteMacUe.cc`** — eNB module chain null check, breaks chained `getSubmodule()` calls into safe steps
- **`simulte/src/stack/mac/scheduler/LteSchedulerEnb.cc`** — `allocatedCws_.at()` guarded with `find()`, prevents `std::out_of_range` under high-load scheduling
- **`simulte/src/stack/mac/layer/LteMacEnb.cc`** — `getCellInfo()` checks submodule existence before cast, returns `nullptr` instead of crashing
- **`simulte/src/corenetwork/lteip/IP2lte.cc`** — `hoManager_` lazy-init in three handover functions checks submodule existence before `check_and_cast`

### Wave 2 — AMC / Scheduler Fixes (found via GDB backtrace analysis)

Discovered while stress-testing Pen30/45/60 under GDB. Root cause: `LteAmc` node-index maps (`dlNodeIndex_`, `ulNodeIndex_`, `d2dNodeIndex_`) and lookup tables can be accessed with a `nodeId`/CQI/MCS value before that UE is fully registered.

- **`simulte/src/stack/mac/amc/LteAmc.cc`** — `existTxParams()`, `getFeedback()`, `setTxParams()` check node-index map membership via `find()` before `.at()`; return safe defaults instead of throwing `std::out_of_range`
- **`simulte/src/stack/mac/amc/LteAmc.cc`** — `getItbsPerCqi()` clamps out-of-range `cqi` to the valid table bound (0–15) before indexing `cqiTable[]`
- **`simulte/src/stack/mac/amc/LteMcs.cc`** — `itbs2tbs()` falls back to `_QPSK` on invalid `LteMod` instead of throwing `cRuntimeError`

### Wave 3 — Vehicle Lifecycle / Stale MacNodeId Fixes

When a vehicle leaves the SUMO map, deleting its OMNeT++ module leaves stale `MacNodeId` references throughout the LTE stack. The workaround is to keep the module alive but disconnected; the remaining fixes guard every path that can still reach a stale or half-torn-down node.

**Base workaround:**

- **`veins/src/veins/modules/mobility/traci/TraCIScenarioManager.cc`** —
  `deleteManagedModule()` no longer calls `callFinish()` / `deleteModule()`.
  Vehicle modules are disconnected from the channel but kept alive in OMNeT++,
  so the LTE stack never receives a "vehicle gone" event.
  **Trade-off:** memory grows with total vehicle churn (see *Known Limitations*).

- **`simulte/src/corenetwork/binder/LteBinder.cc`** — `unregisterNode()`
  proactively detaches the UE from every eNB's AMC (all directions) and clears
  scheduler queues, as a safety net independent of teardown order.

- **`simulte/src/stack/mac/amc/LteAmc.cc`** — `detachUser()` guards against UEs
  never attached in a given direction; `computeTxParams()` returns a static
  invalid sentinel for stale nodeIds, checked via `isSet()` at all 5 downstream
  call sites (`computeReqRbs`, `computeBitsOnNRbs` ×2, `computeBitsOnNRbs_MB`,
  `readCoderate`).

- **`simulte/src/stack/mac/amc/UserTxParams.h`** — added `const isSet()` overload;
  `getCwModulation()` / `getCwRate()` bounds-check CQI against `MAXCQI` before
  indexing `cqiTable[]`.

- **`simulte/src/stack/mac/scheduler/LteSchedulerEnb.cc`** — `scheduleGrant()`
  checks binder liveness for the target nodeId before proceeding.

**Handover path (stale `masterId_` / `candidateMasterId_`):**

Because vehicle modules outlive their SUMO counterparts, a UE's MacNodeId can be
selected as a handover candidate, sending `check_and_cast<LteMacEnb*>` onto a
`LteMacUe` and aborting the run.

- **`simulte/src/stack/phy/layer/LtePhyBase.cc`** — `getAmcModule()` verifies
  `getNodeTypeById(id) == ENODEB` and uses `dynamic_cast<LteMacEnb*>` instead of
  `check_and_cast`, returning `nullptr` for stale or non-eNB ids. Single
  chokepoint protecting every caller.
- **`simulte/src/stack/phy/layer/LtePhyUe.cc`** — `handoverHandler()` rejects
  broadcasts whose `sourceId` is not an eNB before assigning `candidateMasterId_`;
  `doHandover()` aborts cleanly when the candidate is not an eNB or its AMC is
  null; `deleteOldBuffers()` uses `dynamic_cast` before `deleteQueues()`.
- **`simulte/src/stack/phy/layer/LtePhyUeD2D.cc`** — `doHandover()` null-guards
  `oldAmc` / `newAmc` instead of `assert(newAmc != nullptr)`.

**Cell info / antenna set (null deref during handover):**

- **`simulte/src/common/LteCommon.cc`** — `getCellInfo()` uses `dynamic_cast`
  instead of `check_and_cast`; a module may exist without a `cellInfo` submodule
  during handover, and all callers already null-check.
- **`simulte/src/stack/phy/layer/LtePhyUe.cc`** — `doHandover()` aborts if
  `newCellInfo` is null and only calls `cellInfo_->detachUser()` when `cellInfo_`
  is non-null (fixes `SIGSEGV in LteCellInfo::detachUser`).
- **`simulte/src/stack/phy/das/DasFilter.cc`** — `receiveBroadcast()` returns
  early when `ruSet_` is null; `setMasterRuSet()` uses `dynamic_cast<LtePhyEnb*>`
  and clears `das_`/`ruSet_` on mismatch (fixes
  `SIGSEGV in RemoteAntennaSet::getAntennaSetSize`).
- **`simulte/src/stack/phy/layer/LtePhyUe.cc`** — `handleAirFrame()` drops the
  frame when `lteInfo->getUserTxParams()` is null instead of dereferencing it
  (fixes `SIGSEGV in LtePhyUe::handleAirFrame`).

**MAC buffer consistency:**

- **`simulte/src/stack/mac/layer/LteMacUe.cc`**, **`LteMacUeD2D.cc`** — when a
  scheduling grant expects SDUs but the MAC buffer has emptied (stale grant for a
  departed vehicle), the code now logs a warning and `break`s instead of throwing
  `cRuntimeError("Empty buffer for cid ...")`. The affected packets belong to
  vehicles that have already left the network.

**TraCI heap corruption (silent process death):**

Keeping vehicle modules alive meant the application layer kept querying TraCI for
vehicles that no longer exist in SUMO. Each failed query returned a
`Vehicle 'X' is not known` error whose short reply corrupted the Veins TraCI
buffer; after thousands of these, the process died with
`malloc_consolidate(): unaligned fastbin chunk detected` and no OMNeT++ error.

- **`simulte/src/apps/TrafficApp/VehicleApp.cc`**, **`VehicleReRouteApp.cc`** —
  liveness is now checked against
  `TraCIScenarioManager::getManagedHosts().count(externalId)` — a pure in-memory
  map lookup, no TraCI traffic. When the vehicle is gone the app stops reporting
  (and the reroute timer stops rescheduling) without ever touching TraCI.
  This also removed a large per-step overhead: run speed improved several-fold.

### Data Collection Fix

- **`simulte/src/apps/TrafficApp/VehicleApp.cc/.h`** — CSV rows used to be written
  in `finish()`, which never fires under the no-delete workaround. Statistics are
  now written **at the moment the vehicle is detected as gone from SUMO**, using
  the real arrival time, guarded by a `statsWritten` flag against double entries.
  The output directory was also corrected to the current workspace path.

---

## Output

Each run produces two complementary datasets in `simulte/simulations/cars/results/`:

| File | Producer | Contents |
|---|---|---|
| `tripinfo_<Scenario>.xml` | SUMO | Ground-truth per-trip data: `depart`, `arrival`, `duration`, `routeLength`, `waitingTime`, `timeLoss`. Unaffected by the OMNeT++ no-delete workaround. |
| `vehicle_stats_<Scenario>.csv` | `VehicleApp` | LTE-side metrics: `vehicleId, sumoId, isEquipped, departTime, arrivalTime, duration, routesReceived, routesApplied, routesFallback, laneChanges, departEdge, destEdge` |

The two are joined on the SUMO vehicle id. **Use `tripinfo` for travel-time and
distance statistics** (SUMO's exact values) and the CSV for LTE/rerouting
metrics; the CSV's `arrivalTime` is detected at the next report interval and can
lag the true departure by a few seconds.

`tripinfo-output` in `lust.sumo.cfg` is renamed per scenario before each run, e.g.:

```bash
sed -i 's|tripinfo_Penetration30.xml|tripinfo_Penetration45.xml|' lust.sumo.cfg
```

---

## Running a Scenario

```bash
source ~/src/omnetpp-5.7/setenv
export LD_LIBRARY_PATH=$HOME/src/omnetpp-5.7/lib:$HOME/src/inet4/src:$HOME/src/veins/src:$HOME/src/veins_inet/src:$HOME/src/simulte/src

# start the TraCI launcher (one instance only — duplicates corrupt the run)
cd ~/src/simulte/simulations/cars
nohup python3 ~/src/veins/sumo-launchd.py -vv -c sumo > /tmp/launchd.log 2>&1 &

# run inside tmux so the run survives SSH disconnects
tmux new -s sim
opp_run -l ../../src/lte -l ../../../veins/src/veins \
        -l ../../../veins_inet/src/src -l ../../../inet4/src/INET \
        -n "../../src:../../simulations:../../../inet4/src:../../../veins/src/veins:../../../veins_inet/src/veins_inet" \
        -u Cmdenv -c Penetration30 omnetpp.ini
# detach with Ctrl+B then D
```

Monitoring:

```bash
tmux capture-pane -t sim -p | grep "t=" | tail -1
ps aux | grep opp_run | grep -v grep | awk '{print "RAM: "$6/1024/1024" GB"}'
free -h | grep Mem
```

---

## Status

Penetration30 currently runs past **t ≈ 2000 s (~28%)** with no crashes — all
previously fatal points (t ≈ 745, 937, 987, 1233, 1288) are cleared by the Wave 3
fixes. Both `tripinfo` and `vehicle_stats` are being written correctly with real
arrival times.

## Known Limitations

- **Memory growth.** The no-delete workaround keeps every vehicle module in
  memory for the whole run. With the LuST peak demand (~1 700 vehicles entering
  per 5-minute bucket, sustained from t ≈ 1 800 to t ≈ 5 400, peak concurrency
  ≈ 5 100), `opp_run` grows roughly linearly at ~3.5–5 GB per 1 000 simulated
  seconds. A full 7 200 s run approaches the 32 GB limit of the current VM, and
  higher penetration rates (more LTE stacks per vehicle) will exceed it.
  Mitigations under consideration: shortening `sim-time-limit` to cover the peak
  window only, or moving to a constrained-vCPU / high-memory VM
  (e.g. `E8-2ads_v7`: 2 vCPU, 64 GB — fits the existing 4 vCPU quota).
- **Dropped packets under stale grants.** The MAC `Empty buffer` fix skips SDUs
  that a stale grant expected. These belong to vehicles that have already left
  the network, so the effect on results is expected to be negligible, but it is a
  deviation from stock SimuLTE behaviour.
- **CSV arrival lag.** `vehicle_stats` arrival times are detected at the next
  report interval; use `tripinfo` where exact timing matters.

## References

- L. Codecà, R. Frank, S. Faye and T. Engel, "Luxembourg SUMO Traffic (LuST) Scenario: Traffic Demand Evaluation," *IEEE Intelligent Transportation Systems Magazine*, vol. 9, no. 2, pp. 52-63, Summer 2017. [GitHub](https://github.com/lcodeca/LuSTScenario)
- C. Sommer, R. German, and F. Dressler, "Bidirectionally Coupled Network and Road Traffic Simulation for Improved IVC Analysis," *IEEE Transactions on Mobile Computing*, vol. 10, no. 1, pp. 3-15, January 2011. [Veins](https://veins.car2x.org)
- A. Virdis, G. Stea, and G. Nardini, "SimuLTE - A Modular System-level Simulator for LTE/LTE-A Networks based on OMNeT++," *International Conference on Simulation and Modeling Methodologies, Technologies and Applications (SIMULTECH)*, 2014. [SimuLTE](https://simulte.com)
- A. Varga and R. Hornig, "An Overview of the OMNeT++ Simulation Environment," *International ICST Conference on Simulation Tools and Techniques (SIMUTools)*, 2008. [OMNeT++](https://omnetpp.org)
- P. A. Lopez et al., "Microscopic Traffic Simulation using SUMO," *IEEE Intelligent Transportation Systems Conference (ITSC)*, 2018. [SUMO](https://sumo.dlr.de)

## Author

Salih Salur — [GitHub](https://github.com/SalihSalurr)
