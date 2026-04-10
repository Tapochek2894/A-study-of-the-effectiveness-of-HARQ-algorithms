#!/usr/bin/env python3
"""
Построение BER-графиков для HARQ со свёрточными кодами.
Ожидает CSV с колонками: snr_db, chase2_combining, chase3_combining, chase2_no_comb, chase3_no_comb
"""

import argparse
import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

STYLES = {
    "chase2_combining": ("Chase-2 + Combining", "tab:cyan",   "-",  "s"),
    "chase3_combining": ("Chase-3 + Combining", "tab:green",  "-",  "^"),
    "chase2_no_comb":   ("Chase-2", "tab:orange", "--", "s"),
    "chase3_no_comb":   ("Chase-3", "tab:brown",  "--", "^"),
}

def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--csv", default="ber.csv")
    p.add_argument("--out", help="Выходной PNG")
    p.add_argument("--title", help="Заголовок")
    p.add_argument("--log-y", action="store_true", help="Лог-шкала по Y (рекомендуется для BER)")
    p.add_argument("--chase", nargs="+", type=int, choices=[2,3], default=[2,3])
    p.add_argument("--comb-only", action="store_true")
    p.add_argument("--no-comb", action="store_true")
    return p.parse_args()

def read_csv(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            cleaned = {}
            for k, v in row.items():
                if not k: continue
                k = k.strip()
                if v is None or str(v).strip() == "":
                    cleaned[k] = None
                else:
                    try:
                        cleaned[k] = float(v)
                    except:
                        cleaned[k] = v
            rows.append(cleaned)
    return rows

def main():
    args = parse_args()
    path = Path(args.csv)
    if not path.exists():
        sys.exit(f"❌ Файл не найден: {path}")

    rows = read_csv(path)
    if not rows or "snr_db" not in rows[0]:
        sys.exit("❌ Нет данных или отсутствует 'snr_db'")

    snr = [r["snr_db"] for r in rows]
    allowed = set(args.chase)

    cols = []
    for col in STYLES:
        num = int(col.replace("chase","").split("_")[0])
        is_comb = "combining" in col and "no_comb" not in col
        if num not in allowed: continue
        if args.comb_only and not is_comb: continue
        if args.no_comb and is_comb: continue
        if col in rows[0]:
            cols.append(col)

    if not cols:
        sys.exit("❌ Нет данных для отображения")

    plt.figure(figsize=(9, 6))
    for col in cols:
        label, color, ls, marker = STYLES[col]
        x, y = [], []
        for r in rows:
            if r.get(col) is not None:
                x.append(r["snr_db"])
                y.append(r[col])
        if y:
            plt.plot(x, y, color=color, linestyle=ls, marker=marker, 
                    label=label, linewidth=2, markersize=5)

    plt.xlabel("SNR (dB)", fontsize=11)
    plt.ylabel("Bit Error Rate (BER)", fontsize=11)
    plt.title(args.title or "BER Performance — Convolutional Codes", fontsize=13)
    plt.grid(True, linestyle="--", alpha=0.6)
    plt.legend(fontsize=10)
    
    if args.log_y:
        plt.yscale("log")
        plt.ylim(1e-6, 1)  # типичный диапазон для BER
        plt.grid(True, linestyle=":", alpha=0.4, which="minor")

    out = Path(args.out) if args.out else path.with_suffix(".png")
    out.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"✓ Saved: {out}")

if __name__ == "__main__":
    main()