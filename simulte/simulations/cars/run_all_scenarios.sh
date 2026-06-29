#!/bin/bash
SIM_DIR="$(pwd)"
RESULTS_DIR="${SIM_DIR}/results"
INI_FILE="omnetpp.ini"
LTE_LIB="/home/veins/my_workspace/test_workspace/simulte/src/lte"
VEINS_INET_LIB="/home/veins/my_workspace/test_workspace/veins_inet/src/veins_inet"
VEINS_LIB="/home/veins/my_workspace/test_workspace/veins/src/veins"
INET_LIB="/home/veins/my_workspace/test_workspace/inet/src/INET"
NED_PATHS="/home/veins/my_workspace/test_workspace/simulte/src"
NED_PATHS="${NED_PATHS}:/home/veins/my_workspace/test_workspace/simulte/simulations"
NED_PATHS="${NED_PATHS}:/home/veins/my_workspace/test_workspace/inet/src"
NED_PATHS="${NED_PATHS}:/home/veins/my_workspace/test_workspace/veins_inet/src/veins_inet"
NED_PATHS="${NED_PATHS}:/home/veins/my_workspace/test_workspace/veins/src/veins"
NED_PATHS="${NED_PATHS}:/home/veins/my_workspace/test_workspace/simulte/simulations/cars"

SCENARIOS=(
    "Baseline"
    "Penetration05"
    "Penetration10"
    "Penetration15"
    "Penetration30"
)
REPEATS=3

echo "======================================================="
echo "  LTE V2N PENETRASYON DENEYI"
echo "  ${#SCENARIOS[@]} senaryo x ${REPEATS} tekrar = $((${#SCENARIOS[@]} * REPEATS)) calistirma"
echo "  Siralama: her tekrarda tum senaryolar tamamlanir"
echo "======================================================="
echo ""

mkdir -p "${RESULTS_DIR}"
TOTAL_START=$(date +%s)

for RUN in $(seq 0 $((REPEATS - 1))); do
    echo "======================================================="
    echo "  TEKRAR ${RUN}"
    echo "======================================================="

    for SCENARIO in "${SCENARIOS[@]}"; do
        TAG="${SCENARIO}_run${RUN}"

        echo "-------------------------------------------------------"
        echo "  ${TAG} basliyor..."
        echo "-------------------------------------------------------"

        # Onceki gecici dosyalari temizle
        rm -f "${RESULTS_DIR}/traffic_log.txt"
        rm -f "${RESULTS_DIR}/server_log.txt"
        rm -f "${RESULTS_DIR}/tripinfo.xml"
        pkill -f sumo 2>/dev/null || true
        pkill -f TraCI 2>/dev/null || true
        sleep 2

        START_TIME=$(date +%s)

        opp_run \
            -l "${LTE_LIB}" \
            -l "${VEINS_INET_LIB}" \
            -l "${VEINS_LIB}" \
            -l "${INET_LIB}" \
            -n "${NED_PATHS}" \
            -u Cmdenv \
            -c "${SCENARIO}" \
            --seed-set=${RUN} \
            "${INI_FILE}" \
            2>&1 | tee "${RESULTS_DIR}/${TAG}_console.log"

        END_TIME=$(date +%s)
        ELAPSED=$((END_TIME - START_TIME))
        echo ""
        echo "  ${TAG} tamamlandi — $((ELAPSED/60))dk $((ELAPSED%60))s"

        # tripinfo
        if [ -f "${RESULTS_DIR}/tripinfo.xml" ]; then
            mv "${RESULTS_DIR}/tripinfo.xml" "${RESULTS_DIR}/${TAG}_tripinfo.xml"
            TRIP_COUNT=$(grep -c "<tripinfo " "${RESULTS_DIR}/${TAG}_tripinfo.xml" 2>/dev/null || echo 0)
            echo "  + tripinfo -> ${TAG}_tripinfo.xml (${TRIP_COUNT} arac)"
        else
            echo "  - ${TAG}: tripinfo bulunamadi!"
        fi

        # traffic_log
        if [ -f "${RESULTS_DIR}/traffic_log.txt" ]; then
            mv "${RESULTS_DIR}/traffic_log.txt" "${RESULTS_DIR}/${TAG}_traffic_log.txt"
            echo "  + ${TAG}: traffic_log kayitlandi"
        fi

        # server_log
        if [ -f "${RESULTS_DIR}/server_log.txt" ]; then
            mv "${RESULTS_DIR}/server_log.txt" "${RESULTS_DIR}/${TAG}_server_log.txt"
            echo "  + ${TAG}: server_log kayitlandi"
        fi

        pkill -f sumo 2>/dev/null || true
        pkill -f TraCI 2>/dev/null || true
        echo ""
    done

    echo "  Tekrar ${RUN} tamamlandi."
    echo ""
done

# Ozet
TOTAL_END=$(date +%s)
TOTAL_ELAPSED=$((TOTAL_END - TOTAL_START))
echo "======================================================="
echo "  TUM CALISTIRMALAR TAMAMLANDI"
echo "  Toplam sure: $((TOTAL_ELAPSED/60))dk $((TOTAL_ELAPSED%60))s"
echo "======================================================="
echo ""
ls -lh "${RESULTS_DIR}"/*_tripinfo.xml 2>/dev/null || echo "(tripinfo yok)"
echo ""

ANALYZE_SCRIPT="${SIM_DIR}/analyze_penetration.py"
if [ -f "${ANALYZE_SCRIPT}" ]; then
    echo "======================================================="
    echo "  ANALIZ"
    echo "======================================================="
    python3 "${ANALYZE_SCRIPT}" "${RESULTS_DIR}/"
fi

echo ""
echo "  BITTI!"
