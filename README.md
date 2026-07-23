# V2N LTE-Based Traffic Rerouting Simulation

> **⚠️ Active Research:** This project is ongoing. The codebase, configurations, and results are subject to change.

Investigating the impact of **LTE penetration rate** on Vehicle-to-Network (V2N) traffic rerouting effectiveness using the Luxembourg SUMO Traffic (LuST) scenario during peak-hour congestion.

## Overview

This project simulates a realistic urban traffic scenario where a central server collects real-time traffic data from LTE-connected vehicles and issues rerouting commands to alleviate congestion. The key research question: **How does the percentage of LTE-equipped vehicles affect rerouting performance?**

### Simulation Stack

```
┌─────────────┐     TraCI      ┌──────┴───────┐
│    SUMO     │◄──────────────►│   Veins      │
│  (Traffic)  │                │  (V2X Comm)  │
└─────────────┘                └──────┬───────┘
                                      │
                               ┌──────┴───────┐
                               │   SimuLTE    │
                               │  (LTE Stack) │
                               └──────┬───────┘
                                      │
                               ┌──────┴───────┐
                               │    INET      │
                               │  (Network)   │
                               └──────────────┘
```

## Environment

Originally developed on the [Veins virtual machine (Debian-based)](https://veins.car2x.org), which provides a pre-configured environment with OMNeT++, SUMO, and Veins. SimuLTE and INET are added on top.

Long simulation runs are executed on Azure VMs (Ubuntu 24.04) with OMNeT++ 5.7, INET 4, Veins, `veins_inet`, and SimuLTE built from source. With the Wave 5 `deleteModule` fix, RAM usage stays flat regardless of vehicle churn; a 4 vCPU / 16 GB VM is sufficient for all penetration rates including Pen100.

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
- **`simulte/src/apps/TrafficApp/VehicleApp.cc/.h`** — Vehicle application: reports edge ID, speed, travel time to server via UDP/LTE, applies reroute commands, writes per-vehicle CSV statistics, and performs full LTE stack teardown + module deletion on vehicle departure
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

SimuLTE was not designed for the vehicle churn that Veins produces: vehicles are added and removed continuously, and `MacNodeId` references are scattered across the AMC, schedulers, binder, PHY, and DAS filter. The fixes below fall into five waves.

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
  **Trade-off:** memory grew with total vehicle churn — resolved in Wave 5.

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

### Wave 4 — LTE Stack Shutdown (partial resource release)

Root cause identified: `LteMacBase::handleMessage()` reschedules `ttiTick_`
every TTI (1 ms) indefinitely; the only cancellation point was `deleteModule()`,
which was disabled in Wave 3. Every departed vehicle's MAC therefore continued
executing the full MAC cycle ~1000 times per simulated second, consuming both CPU
and RAM.

- **`simulte/src/stack/mac/layer/LteMacBase.h/.cc`** — added public
  `stopTtiTick()` method that cancels the self-scheduled `ttiTick_` message.
- **`simulte/src/apps/TrafficApp/VehicleApp.cc/.h`** — added
  `shutdownLteStack()`, called when the vehicle is detected as gone from SUMO.
  Performs `binder->unregisterNode(nodeId)` (detaches from every eNB's AMC and
  scheduler queues) followed by `mac->stopTtiTick()` (halts the periodic MAC
  cycle).

**Result:** measurable improvement but insufficient — the module's OMNeT++
skeleton (submodule tree, gates, parameter objects) still carried per-event cost
even after TTI and AMC resources were released. This motivated Wave 5.

### Wave 5 — Safe Module Deletion (full resource release)

The definitive fix: actually delete the vehicle's host module (`car[i]`) once the
LTE stack is shut down, eliminating all residual memory and event overhead.

**Challenge:** a module cannot delete itself during its own event processing —
doing so corrupts the OMNeT++ scheduler. The solution uses a deferred
self-message pattern:

- **`simulte/src/apps/TrafficApp/VehicleApp.h`** — added `deleteSelfMsg`
  (cMessage pointer) and `deleteScheduled` flag.
- **`simulte/src/apps/TrafficApp/VehicleApp.cc`** — `shutdownLteStack()` now
  schedules a `"deleteSelf"` self-message at `simTime()` after stopping the MAC
  TTI and unregistering from the binder. The `reportTimer` is cancelled so it
  does not fire on a half-torn-down module.
- **`simulte/src/apps/TrafficApp/VehicleApp.cc`** — `handleMessage()` catches
  the `"deleteSelf"` message (identified by `isSelfMessage()` + name comparison
  to avoid INET `check_and_cast` conflicts), then calls
  `getParentModule()->callFinish()` followed by `deleteModule()` on the host
  module. After `deleteModule()` returns, `this` is destroyed — no member access
  follows.
- **`simulte/src/apps/TrafficApp/VehicleApp.cc`** — `finish()` / destructor
  performs `cancelAndDelete(deleteSelfMsg)` cleanup for modules that are still
  alive at simulation end.

**Safety:** Wave 3 guards protect every code path that could reach a stale
`MacNodeId` after deletion. `unregisterNode()` detaches the UE from all eNB AMCs
and scheduler queues *before* the module is destroyed, so no dangling reference
remains. The `"deleteSelf"` message is identified by name (`strcmp`) rather than
pointer comparison, avoiding INET socket handler `check_and_cast` conflicts.

**Result:**

| Metric | Before (Wave 4 only) | After (Wave 5) |
|---|---|---|
| Present messages at t=5400 | ~27 million | ~2 million |
| RAM at t=5400 (Pen60) | 160 GB | ~10 GB |
| RAM growth | Unbounded (OOM at t≈6000) | Flat (bounded) |

With Wave 5, penetration rates up to Pen100 can run to completion on a 16 GB VM.

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
| `tripinfo_<Scenario>.xml` | SUMO | Ground-truth per-trip data: `depart`, `arrival`, `duration`, `routeLength`, `waitingTime`, `timeLoss`. Unaffected by the OMNeT++ module lifecycle. |
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

# Optional: use jemalloc for reduced memory fragmentation and improved speed
export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so.2
export MALLOC_CONF="background_thread:true,dirty_decay_ms:5000,muzzy_decay_ms:5000"

# start the TraCI launcher (one instance only — duplicates corrupt the run)
cd ~/src/simulte/simulations/cars
nohup python3 ~/src/veins/sumo-launchd.py -vv -c sumo > /tmp/launchd.log 2>&1 &

# run inside tmux so the run survives SSH disconnects
tmux new -s sim
opp_run -l ../../src/lte -l ../../../veins/src/veins \
        -l ../../../veins_inet/src/src -l ../../../inet4/src/INET \
        -n "../../src:../../simulations:../../../inet4/src:../../../veins/src/veins:../../../veins_inet/src/veins_inet" \
        -u Cmdenv -c Penetration30 omnetpp.ini 2>&1 | tee /tmp/sim.log
# detach with Ctrl+B then D
```

Monitoring:

```bash
# Simulation progress + RAM
tmux capture-pane -t sim -p | tail -5
free -h | grep Mem

# Check for errors
grep -iE 'error|segfault|abort' /tmp/sim.log | head

# Verify jemalloc is loaded
PID=$(pgrep -f opp_run); grep -q jemalloc /proc/$PID/maps && echo "jemalloc ACTIVE"
```

### Performance Tips

- **jemalloc** (`LD_PRELOAD`): reduces heap fragmentation from millions of
  message alloc/free cycles; ~10–25 % speed improvement observed.
- **Release build**: ensure `libsrc.so` (not `libsrc_dbg.so`) is on
  `LD_LIBRARY_PATH`; debug builds are 3–5× slower.
- **CPU governor**: `echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor`
  prevents throttling on cloud VMs.

---

## References

- L. Codecà, R. Frank, S. Faye and T. Engel, "Luxembourg SUMO Traffic (LuST) Scenario: Traffic Demand Evaluation," *IEEE Intelligent Transportation Systems Magazine*, vol. 9, no. 2, pp. 52-63, Summer 2017. [GitHub](https://github.com/lcodeca/LuSTScenario)
- C. Sommer, R. German, and F. Dressler, "Bidirectionally Coupled Network and Road Traffic Simulation for Improved IVC Analysis," *IEEE Transactions on Mobile Computing*, vol. 10, no. 1, pp. 3-15, January 2011. [Veins](https://veins.car2x.org)
- A. Virdis, G. Stea, and G. Nardini, "SimuLTE - A Modular System-level Simulator for LTE/LTE-A Networks based on OMNeT++," *International Conference on Simulation and Modeling Methodologies, Technologies and Applications (SIMULTECH)*, 2014. [SimuLTE](https://simulte.com)
- A. Varga and R. Hornig, "An Overview of the OMNeT++ Simulation Environment," *International ICST Conference on Simulation Tools and Techniques (SIMUTools)*, 2008. [OMNeT++](https://omnetpp.org)
- P. A. Lopez et al., "Microscopic Traffic Simulation using SUMO," *IEEE Intelligent Transportation Systems Conference (ITSC)*, 2018. [SUMO](https://sumo.dlr.de)

## Author

Salih Salur — [GitHub](https://github.com/SalihSalurr)
