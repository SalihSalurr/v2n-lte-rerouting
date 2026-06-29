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
echo "  Siralama: her tekrarda 4 senaryo, 2li paralel"
echo "======================================================="
echo ""

mkdir -p "${RESULTS_DIR}"
TOTAL_START=$(date +%s)

# ── Tek bir çalıştırma fonksiyonu ──────────────────────────
run_once() {
    local SCENARIO=$1
    local RUN=$2
    local TAG="${SCENARIO}_run${RUN}"
    local START_TIME=$(date +%s)

    echo "  >> ${TAG} basliyor..."

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
        2>&1 | tee "${RESULTS_DIR}/${TAG}_console.log" | sed "s/^/[${TAG}] /"

    local END_TIME=$(date +%s)
    local ELAPSED=$((END_TIME - START_TIME))
    echo "  << ${TAG} tamamlandi — $((ELAPSED/60))dk $((ELAPSED%60))s"

    # tripinfo
    if [ -f "${RESULTS_DIR}/tripinfo.xml" ]; then
        mv "${RESULTS_DIR}/tripinfo.xml" "${RESULTS_DIR}/${TAG}_tripinfo.xml"
        local TRIP_COUNT=$(grep -c "<tripinfo " "${RESULTS_DIR}/${TAG}_tripinfo.xml" 2>/dev/null || echo 0)
        echo "  + ${TAG}: tripinfo kayitlandi (${TRIP_COUNT} arac)"
    else
        echo "  - ${TAG}: tripinfo bulunamadi!"
    fi

    if [ -f "${RESULTS_DIR}/traffic_log.txt" ]; then
        mv "${RESULTS_DIR}/traffic_log.txt" "${RESULTS_DIR}/${TAG}_traffic_log.txt"
        echo "  + ${TAG}: traffic_log kayitlandi"
    fi

    if [ -f "${RESULTS_DIR}/server_log.txt" ]; then
        mv "${RESULTS_DIR}/server_log.txt" "${RESULTS_DIR}/${TAG}_server_log.txt"
        echo "  + ${TAG}: server_log kayitlandi"
    fi
}

# ── Sıralama: run0 tüm senaryolar, run1 tüm senaryolar... ──
# Her tekrarda 4 senaryo 2li gruplara ayrılır:
# Grup A: Baseline + Penetration05
# Grup B: Penetration10 + Penetration15

GROUP=1
for RUN in $(seq 0 $((REPEATS - 1))); do
    echo "======================================================="
    echo "  TEKRAR ${RUN} / $((REPEATS - 1))"
    echo "======================================================="

    echo "-------------------------------------------------------"
    echo "  GRUP ${GROUP}: Baseline_run${RUN} + Penetration05_run${RUN}"
    echo "-------------------------------------------------------"
    run_once "Baseline" ${RUN} &
    PID1=$!
    run_once "Penetration05" ${RUN} &
    PID2=$!
    wait $PID1
    wait $PID2
    echo "  Grup ${GROUP} tamamlandi."
    echo ""
    GROUP=$((GROUP + 1))

    echo "-------------------------------------------------------"
    echo "  GRUP ${GROUP}: Penetration10_run${RUN} + Penetration15_run${RUN}"
    echo "-------------------------------------------------------"
    run_once "Penetration10" ${RUN} &
    PID3=$!
    run_once "Penetration15" ${RUN} &
    PID4=$!
    wait $PID3
    wait $PID4
    echo "  Grup ${GROUP} tamamlandi."
    echo ""
    GROUP=$((GROUP + 1))

done

# ── Özet ──────────────────────────────────────────────────
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

