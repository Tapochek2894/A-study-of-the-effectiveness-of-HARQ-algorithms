#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np

EPSILON = 1e-7

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
            c1_val = float(row["chase1_comb_ber"])
            data["c1"].append(np.nan if c1_val < EPSILON else c1_val)
            
            c2_val = float(row["chase2_comb_ber"])
            data["c2"].append(np.nan if c2_val < EPSILON else c2_val)
            
            c3_val = float(row["chase3_comb_ber"])
            data["c3"].append(np.nan if c3_val < EPSILON else c3_val)
            
            nc1_val = float(row["chase1_nocomb_ber"])
            data["nc1"].append(np.nan if nc1_val < EPSILON else nc1_val)
            
            nc2_val = float(row["chase2_nocomb_ber"])
            data["nc2"].append(np.nan if nc2_val < EPSILON else nc2_val)
            
            nc3_val = float(row["chase3_nocomb_ber"])
            data["nc3"].append(np.nan if nc3_val < EPSILON else nc3_val)
    
    if not data["snr"]:
        print("Error: No data in CSV")
        return

    fig, ax = plt.subplots(figsize=(9, 6))
    
    # === Цвета: один базовый цвет на алгоритм ===
    # Ключи: "c1", "c2", "c3" (номер алгоритма Chase)
    colors = {
        "c1": ("#1f5fbf", "#b502c5"),  # Chase-1: тёмно-синий / светло-синий
        "c2": ("#1f8f3f", "#46c51f"),  # Chase-2: тёмно-зелёный / светло-зелёный
        "c3": ("#bf2f2f", "#ff0000"),  # Chase-3: тёмно-красный / светло-красный
    }
    
    # Маркеры для разных алгоритмов
    markers = {
        "c1": "o",     # Chase-1: кружок
        "c2": "s",     # Chase-2: квадрат
        "c3": "^",     # Chase-3: треугольник
    }
    
    labels = {
        "c1": "Chase-1 + combining", "c2": "Chase-2 + combining", "c3": "Chase-3 + combining",
        "nc1": "Chase-1", "nc2": "Chase-2", "nc3": "Chase-3"
    }

    # === Плотим кривые с комбинированием (тёмные цвета) ===
    if not args.nocomb:
        for key in ["c1", "c2", "c3"]:
            # Фильтруем NaN значения
            mask = ~np.isnan(data[key])
            if np.any(mask):
                snr_clean = np.array(data["snr"])[mask]
                ber_clean = np.array(data[key])[mask]
                
                ax.semilogy(snr_clean, ber_clean,
                       color=colors[key][0],           # тёмный оттенок
                       marker=markers[key],            # маркер
                       markersize=6,                    # размер маркера
                       markevery=1,                     # маркер на каждой точке
                       label=labels[key],
                       linewidth=2.0,
                       linestyle='-')
    
    # === Плотим кривые без комбинирования (светлые цвета) ===
    if not args.comb:
        for key in ["nc1", "nc2", "nc3"]:
            # Фильтруем NaN значения
            mask = ~np.isnan(data[key])
            if np.any(mask):
                snr_clean = np.array(data["snr"])[mask]
                ber_clean = np.array(data[key])[mask]
                color_key = key.replace("nc", "c")  # "nc1" → "c1"
                
                ax.semilogy(snr_clean, ber_clean,
                       color=colors[color_key][1],   # светлый оттенок
                       marker=markers[color_key],    # тот же маркер, что и для combining
                       markersize=6,                   # размер маркера
                       markevery=1,                    # маркер на каждой точке
                       label=labels[key],
                       linewidth=1.5,
                       alpha=0.7,
                       linestyle='-')

    ax.set_xlabel("SNR (dB)", fontsize=11)
    ax.set_ylabel("Bit Error Rate (BER)", fontsize=11)
    ax.set_title("Chase Combining: BER vs SNR", fontsize=13, pad=15)
    ax.grid(True, which="both", linestyle="--", alpha=0.6)
    ax.set_axisbelow(True)
    ax.legend(fontsize=9, ncol=2 if not (args.comb or args.nocomb) else 1)
    
    out = args.out or args.csv_path.with_name(args.csv_path.stem + "ber_plot.png")
    fig.tight_layout()
    plt.show()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")

if __name__ == "__main__":
    main()