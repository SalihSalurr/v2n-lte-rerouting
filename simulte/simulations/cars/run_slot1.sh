#!/bin/bash
OPP_RUN="opp_run -l /home/veins/my_workspace/test_workspace/simulte/src/lte -l /home/veins/my_workspace/test_workspace/veins_inet/src/veins_inet -l /home/veins/my_workspace/test_workspace/veins/src/veins -l /home/veins/my_workspace/test_workspace/inet/src/INET -n /home/veins/my_workspace/test_workspace/simulte/src:/home/veins/my_workspace/test_workspace/simulte/simulations:/home/veins/my_workspace/test_workspace/inet/src:/home/veins/my_workspace/test_workspace/veins_inet/src/veins_inet:/home/veins/my_workspace/test_workspace/veins/src/veins:/home/veins/my_workspace/test_workspace/simulte/simulations/cars -u Cmdenv"
SCENARIOS=("Baseline" "Penetration15" "Penetration45" "Penetration60")
SLOT_CFG="istanbul-v1-slot1.sumo.cfg"
cd /home/veins/my_workspace/test_workspace/simulte/simulations/cars || exit 1
mkdir -p results
echo "==== SLOT1 BASLIYOR ==== $(date)"
for SCENARIO in "${SCENARIOS[@]}"; do
    echo ">>> $SCENARIO basliyor"
    sed -i "s|<tripinfo-output value=\".*\"/>|<tripinfo-output value=\"/home/veins/my_workspace/test_workspace/simulte/simulations/cars/results/tripinfo_${SCENARIO}.xml\"/>|" "$SLOT_CFG"
    $OPP_RUN -c "$SCENARIO" omnetpp.ini 2>&1 | tee "results/${SCENARIO}.log"
    echo ">>> $SCENARIO tamamlandi"
    COUNT=$(grep -c "<tripinfo " "results/tripinfo_${SCENARIO}.xml" 2>/dev/null || echo "0")
    echo ">>> Tamamlanan arac sayisi: $COUNT"
done
echo "==== SLOT1 TAMAMLANDI ===="
