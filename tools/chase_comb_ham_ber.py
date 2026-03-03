#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path", type=Path, help="Input CSV from harq_ber_sim")
    parser.add_argument("--out", "-o", type=Path, default=None)
    parser.add_argument("--comb", action="store_true", help="Plot combining only")
    parser.add_argument("--nocomb", action="store_true", help="Plot no-combining only")
    args = parser.parse_args()

    data = {"snr": [], "c1": [], "c2": [], "c3": [], "nc1": [], "nc2": [], "nc3": []}
    
    with args.csv_path.open() as f:
        reader = csv.DictReader(f)
        for row in reader:
            data["snr"].append(float(row["snr_db"]))
            data["c1"].append(float(row["chase1_comb_ber"]))
            data["c2"].append(float(row["chase2_comb_ber"]))
            data["c3"].append(float(row["chase3_comb_ber"]))
            data["nc1"].append(float(row["chase1_nocomb_ber"]))
            data["nc2"].append(float(row["chase2_nocomb_ber"]))
            data["nc3"].append(float(row["chase3_nocomb_ber"]))

    if not data["snr"]:
        print("Error: No data in CSV")
        return

    fig, ax = plt.subplots(figsize=(9, 6))
    
    # === Цвета: один базовый цвет на алгоритм ===
    # Ключи: "c1", "c2", "c3" (номер алгоритма Chase)
    colors = {
        "c1": ("#1f5fbf", "#3b75e7"),  # Chase-1: тёмно-синий / светло-синий
        "c2": ("#1f8f3f", "#40b87c"),  # Chase-2: тёмно-зелёный / светло-зелёный
        "c3": ("#bf2f2f", "#be4848"),  # Chase-3: тёмно-красный / светло-красный
    }
    
    # Стили линий
    linestyles = {
        "c1": "-",    # Chase-1: сплошная
        "c2": "--",   # Chase-2: пунктир
        "c3": "-.",   # Chase-3: штрихпунктир
    }
    
    labels = {
        "c1": "Chase-1 + combining", "c2": "Chase-2 + combining", "c3": "Chase-3 + combining",
        "nc1": "Chase-1", "nc2": "Chase-2", "nc3": "Chase-3"
    }

    # === Плотим кривые с комбинированием (тёмные цвета) ===
    if not args.nocomb:
        for key in ["c1", "c2", "c3"]:
            if data[key] and any(v > 0 for v in data[key]):
                ax.plot(data["snr"], data[key],
                       color=colors[key][0],           # тёмный оттенок
                       linestyle=linestyles[key],
                       label=labels[key],
                       linewidth=2.0,
                       markersize=4,
                       markevery=5)
    
    # === Плотим кривые без комбинирования (светлые цвета) ===
    if not args.comb:
        for key in ["nc1", "nc2", "nc3"]:
            if data[key] and any(v > 0 for v in data[key]):
                # === ИСПРАВЛЕНИЕ: маппим nc1→c1, nc2→c2, nc3→c3 ===
                color_key = key.replace("nc", "c")  # "nc1" → "c1"
                ax.plot(data["snr"], data[key],
                       color=colors[color_key][1],   # светлый оттенок
                       linestyle=linestyles[color_key],
                       label=labels[key],
                       linewidth=1.5,
                       alpha=0.7,
                       markersize=4,
                       markevery=5)

    ax.set_xlabel("SNR (dB)", fontsize=11)
    ax.set_ylabel("Bit Error Rate (BER)", fontsize=11)
    ax.set_title("HARQ with Chase Combining: BER vs SNR", fontsize=13, pad=15)
    ax.grid(True, which="both", linestyle="--", alpha=0.6)
    ax.set_axisbelow(True)
    ax.legend(fontsize=9, ncol=2 if not (args.comb or args.nocomb) else 1)
    
    out = args.out or args.csv_path.with_name(args.csv_path.stem + "_ber_plot_linear.png")
    fig.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"✓ Saved: {out}")

if __name__ == "__main__":
    main()