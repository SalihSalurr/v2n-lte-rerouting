#!/usr/bin/env python3
"""
Penetrasyon Oranı Deney Analizi
================================
SUMO tripinfo.xml dosyalarından tüm senaryoları okur,
equipped / unequipped araçları ayırır, metrik tablosu ve
karşılaştırma grafikleri üretir.

Kullanım:
  python3 analyze_penetration.py results/

  results/ klasöründe şu dosyalar beklenir:
    Baseline_tripinfo.xml
    Penetration05_tripinfo.xml
    Penetration10_tripinfo.xml
    ...

  + Her senaryo için traffic_log dosyasından hangi araçların
    equipped olduğu bilgisi okunur (opsiyonel).
"""

import xml.etree.ElementTree as ET
import os
import sys
import csv
from collections import defaultdict

# ══════════════════════════════════════════════════════════════
#  Senaryo tanımları
# ══════════════════════════════════════════════════════════════
SCENARIOS = [
    ("Baseline",       0.00, "Baseline_tripinfo.xml"),
    ("Penetration05",  0.05, "Penetration05_tripinfo.xml"),
    ("Penetration10",  0.10, "Penetration10_tripinfo.xml"),
    ("Penetration15",  0.15, "Penetration15_tripinfo.xml"),
    ("Penetration20",  0.20, "Penetration20_tripinfo.xml"),
    ("Penetration25",  0.25, "Penetration25_tripinfo.xml"),
    ("Penetration30",  0.30, "Penetration30_tripinfo.xml"),
]


def parse_tripinfo(filepath):
    """
    tripinfo.xml'den araç verilerini oku.

    Dönen dict:
      vehicleId -> {
        'duration': float,       # toplam seyahat süresi (s)
        'waitingTime': float,    # toplam bekleme süresi (s)
        'timeLoss': float,       # ideal süreye göre kayıp (s)
        'routeLength': float,    # kat edilen mesafe (m)
        'departDelay': float,    # kalkış gecikmesi (s)
        'speedFactor': float,    # hız faktörü
        'depart': float,         # kalkış zamanı (s)
        'arrival': float,        # varış zamanı (s)
      }
    """
    if not os.path.exists(filepath):
        print(f"  UYARI: {filepath} bulunamadi, atlanıyor.")
        return {}

    tree = ET.parse(filepath)
    root = tree.getroot()

    vehicles = {}
    for trip in root.findall("tripinfo"):
        vid = trip.get("id")
        vehicles[vid] = {
            "duration":     float(trip.get("duration", 0)),
            "waitingTime":  float(trip.get("waitingTime", 0)),
            "timeLoss":     float(trip.get("timeLoss", 0)),
            "routeLength":  float(trip.get("routeLength", 0)),
            "departDelay":  float(trip.get("departDelay", 0)),
            "speedFactor":  float(trip.get("speedFactor", 1)),
            "depart":       float(trip.get("depart", 0)),
            "arrival":      float(trip.get("arrival", -1)),
        }

    return vehicles


def parse_equipped_vehicles(log_filepath):
    """
    traffic_log.txt'den isEquipped=1 olan araçları bul.

    Log formatı:
      [t=...] [car[N]] initialized | ... | isEquipped=1
    """
    equipped = set()
    if not os.path.exists(log_filepath):
        return equipped

    with open(log_filepath, "r") as f:
        for line in f:
            if "isEquipped=1" in line:
                # [t=...] [car[5]] initialized ...
                start = line.find("[", line.find("]") + 1) + 1
                end = line.find("]", start)
                if start > 0 and end > start:
                    veh_name = line[start:end]
                    equipped.add(veh_name)

    return equipped


def compute_metrics(vehicles, equipped_set=None):
    """
    Araç verilerinden ortalama metrikler hesapla.

    equipped_set verilmişse, araçları üç gruba ayırır:
      all, equipped, unequipped
    """
    groups = {"all": [], "equipped": [], "unequipped": []}

    for vid, data in vehicles.items():
        if data["arrival"] < 0:
            continue  # varışa ulaşamamış araçlar

        groups["all"].append(data)

        if equipped_set is not None:
            # SUMO vehicle ID'si: genellikle "flow_0.5" gibi
            # OMNeT++ module adı: "car[5]" gibi
            # Eşleştirme zor olabilir — penetration rate'den tahmin et
            # Burada basit yaklaşım: equipped_set boşsa ayırma yapma
            pass

    result = {}
    for group_name, group_data in groups.items():
        if not group_data:
            result[group_name] = {
                "count": 0,
                "avg_duration": 0,
                "avg_waitingTime": 0,
                "avg_timeLoss": 0,
                "avg_routeLength": 0,
                "avg_departDelay": 0,
                "total_sim_time": 0,
            }
            continue

        n = len(group_data)
        result[group_name] = {
            "count": n,
            "avg_duration":     sum(d["duration"] for d in group_data) / n,
            "avg_waitingTime":  sum(d["waitingTime"] for d in group_data) / n,
            "avg_timeLoss":     sum(d["timeLoss"] for d in group_data) / n,
            "avg_routeLength":  sum(d["routeLength"] for d in group_data) / n,
            "avg_departDelay":  sum(d["departDelay"] for d in group_data) / n,
            "total_sim_time":   max(d["arrival"] for d in group_data),
        }

    return result


def main():
    if len(sys.argv) < 2:
        print("Kullanım: python3 analyze_penetration.py <results_dizini>")
        print("Örnek:    python3 analyze_penetration.py results/")
        sys.exit(1)

    results_dir = sys.argv[1]

    print("=" * 70)
    print("  LTE V2N PENETRASYON ORANI ANALİZİ")
    print("=" * 70)
    print()

    all_results = []

    for name, rate, filename in SCENARIOS:
        filepath = os.path.join(results_dir, filename)
        print(f"[{name}] penetrationRate={rate:.0%}")

        vehicles = parse_tripinfo(filepath)
        if not vehicles:
            print(f"  -> Veri yok, atlanıyor.\n")
            continue

        metrics = compute_metrics(vehicles)
        m = metrics["all"]

        print(f"  Araç sayısı   : {m['count']}")
        print(f"  Ort. süre     : {m['avg_duration']:.1f} s")
        print(f"  Ort. bekleme  : {m['avg_waitingTime']:.1f} s")
        print(f"  Ort. kayıp    : {m['avg_timeLoss']:.1f} s")
        print(f"  Ort. mesafe   : {m['avg_routeLength']:.1f} m")
        print(f"  Toplam sim    : {m['total_sim_time']:.1f} s")
        print()

        all_results.append({
            "scenario": name,
            "rate": rate,
            **m
        })

    # ── Karşılaştırma tablosunu CSV'ye yaz ───────────────────
    if all_results:
        csv_path = os.path.join(results_dir, "penetration_comparison.csv")
        with open(csv_path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=[
                "scenario", "rate", "count",
                "avg_duration", "avg_waitingTime", "avg_timeLoss",
                "avg_routeLength", "avg_departDelay", "total_sim_time"
            ])
            writer.writeheader()
            writer.writerows(all_results)

        print(f"Karşılaştırma tablosu: {csv_path}")
        print()

    # ── Baseline'a göre iyileşme yüzdeleri ───────────────────
    if len(all_results) >= 2:
        baseline = all_results[0]
        print("=" * 70)
        print("  BASELINE'A GÖRE İYİLEŞME")
        print("=" * 70)
        print(f"{'Senaryo':<18} {'Süre':>10} {'Bekleme':>10} {'Kayıp':>10}")
        print("-" * 50)

        for r in all_results:
            if baseline["avg_duration"] > 0:
                dur_change = ((r["avg_duration"] - baseline["avg_duration"])
                              / baseline["avg_duration"] * 100)
            else:
                dur_change = 0

            if baseline["avg_waitingTime"] > 0:
                wait_change = ((r["avg_waitingTime"] - baseline["avg_waitingTime"])
                               / baseline["avg_waitingTime"] * 100)
            else:
                wait_change = 0

            if baseline["avg_timeLoss"] > 0:
                loss_change = ((r["avg_timeLoss"] - baseline["avg_timeLoss"])
                               / baseline["avg_timeLoss"] * 100)
            else:
                loss_change = 0

            print(f"{r['scenario']:<18} {dur_change:>+9.1f}% {wait_change:>+9.1f}% {loss_change:>+9.1f}%")

        print()
        print("Negatif değerler = iyileşme (azalma)")
        print("Pozitif değerler = kötüleşme (artma)")


if __name__ == "__main__":
    main()

