#!/bin/bash
SCENARIOS=(Baseline Penetration15)
SIMDIR=/home/veins/my_workspace/test_workspace/simulte/simulations/cars
OPP="opp_run -l /home/veins/my_workspace/test_workspace/simulte/src/lte -l /home/veins/my_workspace/test_workspace/simulte/../veins_inet/src/veins_inet -l /home/veins/my_workspace/test_workspace/simulte/../veins/src/veins -l /home/veins/my_workspace/test_workspace/simulte/../inet/src/INET -n /home/veins/my_workspace/test_workspace/simulte/src:/home/veins/my_workspace/test_workspace/simulte/simulations:/home/veins/my_workspace/test_workspace/simulte/../inet/src:/home/veins/my_workspace/test_workspace/simulte/../veins_inet/src/veins_inet:/home/veins/my_workspace/test_workspace/simulte/../veins/src/veins:/home/veins/my_workspace/test_workspace/simulte/simulations/cars -u Cmdenv"
cd $SIMDIR || exit 1
mkdir -p results
for S in "${SCENARIOS[@]}"; do
  echo ">>> $S basliyor — $(date)"
  pkill -f sumo 2>/dev/null; sleep 2
  sed -i "s|<tripinfo-output value=\"[^\"]*\"/>|<tripinfo-output value=\"$SIMDIR/results/tripinfo_${S}.xml\"/>|" lust.sumo.cfg
  $OPP -c "$S" omnetpp.ini 2>&1 | tee "results/${S}.log"
  echo ">>> $S tamamlandi — $(date)"
done
echo "Tum senaryolar bitti — $(date)"
