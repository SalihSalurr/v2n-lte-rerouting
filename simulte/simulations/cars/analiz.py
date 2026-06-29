#!/usr/bin/env python3
"""
Tripinfo XML analizi — eski tablo formatinda yeni sonuclari uretir.

Kullanim:
    python3 analyze_tripinfo.py

Beklenen dosyalar (results/ icinde):
    tripinfo_Baseline.xml
    tripinfo_Penetration05.xml
    tripinfo_Penetration10.xml
    tripinfo_Penetration15.xml
    tripinfo_Penetration30.xml

Eksik dosyalar atlanir.
"""

import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

# ────────────────────────────────────────────────────────────
# AYARLAR
# ────────────────────────────────────────────────────────────
RESULTS_DIR = "/home/veins/my_workspace/test_workspace/simulte/simulations/cars/results"

SCENARIOS = [
    ("Baseline",       "tripinfo_Baseline.xml"),
    ("Penetration %5", "tripinfo_Penetration05.xml"),
    ("Penetration %10","tripinfo_Penetration10.xml"),
    ("Penetration %15","tripinfo_Penetration15.xml"),
    ("Penetration %30","tripinfo_Penetration30.xml"),
]

# ────────────────────────────────────────────────────────────

def parse_tripinfo(path):
    """Bir tripinfo XML dosyasini parse eder, ortalamalari dondurur."""
    if not os.path.isfile(path):
        return None

    n = 0
    sum_duration = 0.0
    sum_waiting  = 0.0
    sum_loss     = 0.0
    sum_route    = 0.0

    # iterparse — dosya buyuk olabilir, streaming parse
    try:
        for event, elem in ET.iterparse(path, events=("end",)):
            if elem.tag != "tripinfo":
                continue
            try:
                duration = float(elem.attrib.get("duration", 0))
                waiting  = float(elem.attrib.get("waitingTime", 0))
                loss     = float(elem.attrib.get("timeLoss", 0))
                route    = float(elem.attrib.get("routeLength", 0))
            except ValueError:
                elem.clear()
                continue

            sum_duration += duration
            sum_waiting  += waiting
            sum_loss     += loss
            sum_route    += route
            n += 1

            elem.clear()
    except ET.ParseError as e:
        print(f"  ⚠️  XML parse hatasi: {e}")
        if n == 0:
            return None

    if n == 0:
        return None

    return {
        "n":         n,
        "duration":  sum_duration / n,
        "waiting":   sum_waiting  / n,
        "loss":      sum_loss     / n,
        "route":     sum_route    / n,
    }


def fmt_diff(new_val, base_val, decimals=1):
    """Baseline ile farkı +/- isaretli string olarak dondurur."""
    if base_val is None:
        return ""
    diff = new_val - base_val
    sign = "+" if diff >= 0 else ""
    return f" {sign}{diff:.{decimals}f}"


def main():
    print(f"\nResults dizini: {RESULTS_DIR}\n")

    # Tum senaryolari oku
    results = {}
    for label, fname in SCENARIOS:
        path = os.path.join(RESULTS_DIR, fname)
        print(f"  Okunuyor: {fname} ... ", end="", flush=True)
        r = parse_tripinfo(path)
        if r is None:
            print("YOK / BOS")
        else:
            print(f"OK ({r['n']} arac)")
        results[label] = r

    print()

    # Baseline'i referans olarak al
    base = results.get("Baseline")
    if base is None:
        print("⚠️  Baseline yok — diff'siz tablo cikiyor.")

    # ────────────────────────────────────────────────────────
    #  KONSOL TABLOSU
    # ────────────────────────────────────────────────────────
    print("=" * 100)
    print(f"{'Senaryo':<18} {'Araç':>6} {'Ort. Süre (s)':>20} {'Ort. Bekleme (s)':>22} {'Ort. Kayıp (s)':>20} {'Ort. Mesafe (m)':>22}")
    print("=" * 100)

    for label, _ in SCENARIOS:
        r = results.get(label)
        if r is None:
            print(f"{label:<18} {'—':>6} {'—':>20} {'—':>22} {'—':>20} {'—':>22}")
            continue

        if label == "Baseline" or base is None:
            duration_s = f"{r['duration']:.1f}"
            waiting_s  = f"{r['waiting']:.1f}"
            loss_s     = f"{r['loss']:.1f}"
            route_s    = f"{r['route']:.1f}"
        else:
            duration_s = f"{r['duration']:.1f}{fmt_diff(r['duration'], base['duration'])}"
            waiting_s  = f"{r['waiting']:.1f}{fmt_diff(r['waiting'],  base['waiting'])}"
            loss_s     = f"{r['loss']:.1f}{fmt_diff(r['loss'],     base['loss'])}"
            route_s    = f"{r['route']:.1f}{fmt_diff(r['route'],    base['route'])}"

        print(f"{label:<18} {r['n']:>6} {duration_s:>20} {waiting_s:>22} {loss_s:>20} {route_s:>22}")

    print("=" * 100)

    # ────────────────────────────────────────────────────────
    #  CSV CIKTI (ekstra, kopyala-yapistir icin)
    # ────────────────────────────────────────────────────────
    csv_path = os.path.join(RESULTS_DIR, "summary_table.csv")
    with open(csv_path, "w") as f:
        f.write("Senaryo,Arac,Ort_Sure_s,Ort_Bekleme_s,Ort_Kayip_s,Ort_Mesafe_m,"
                "Sure_Diff,Bekleme_Diff,Kayip_Diff,Mesafe_Diff\n")
        for label, _ in SCENARIOS:
            r = results.get(label)
            if r is None:
                f.write(f"{label},,,,,,,,,\n")
                continue
            if label == "Baseline" or base is None:
                f.write(f"{label},{r['n']},{r['duration']:.2f},{r['waiting']:.2f},"
                        f"{r['loss']:.2f},{r['route']:.2f},,,,\n")
            else:
                f.write(f"{label},{r['n']},{r['duration']:.2f},{r['waiting']:.2f},"
                        f"{r['loss']:.2f},{r['route']:.2f},"
                        f"{r['duration']-base['duration']:+.2f},"
                        f"{r['waiting']-base['waiting']:+.2f},"
                        f"{r['loss']-base['loss']:+.2f},"
                        f"{r['route']-base['route']:+.2f}\n")

    print(f"\n  CSV ozet: {csv_path}\n")

    # ────────────────────────────────────────────────────────
    #  ESKI TABLO ILE KARSILASTIRMA (bilgi amacli)
    # ────────────────────────────────────────────────────────
    OLD_TABLE = {
        "Baseline":       (6999, 388.1, 85.8,  157.3, 3108.3),
        "Penetration %5": (6973, 383.0, 80.7,  152.6, 3096.1),
        "Penetration %10":(6979, 387.2, 83.8,  155.5, 3104.3),
        "Penetration %15":(6798, 400.9, 97.1,  168.4, 3081.0),
    }

    print("\n" + "=" * 100)
    print("ESKI ALGORITMA vs YENI ALGORITMA (Ort. Süre)")
    print("=" * 100)
    print(f"{'Senaryo':<18} {'Eski':>10} {'Yeni':>10} {'Fark':>12}  {'Yorum':>15}")
    print("-" * 100)
    for label, _ in SCENARIOS:
        r = results.get(label)
        if r is None or label not in OLD_TABLE:
            continue
        old_n, old_dur, _, _, _ = OLD_TABLE[label]
        new_dur = r["duration"]
        diff = new_dur - old_dur
        sign = "+" if diff >= 0 else ""
        comment = "✅ iyilesti" if diff < -0.5 else ("❌ kotulesti" if diff > 0.5 else "≈ ayni")
        print(f"{label:<18} {old_dur:>10.1f} {new_dur:>10.1f} {sign}{diff:>+10.1f}  {comment:>15}")
    print("=" * 100)


if __name__ == "__main__":
    main()
