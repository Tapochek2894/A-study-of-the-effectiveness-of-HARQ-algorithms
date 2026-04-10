#!/usr/bin/env python3
"""
Построение графика среднего числа повторных передач (HARQ, свёрточные коды).
По умолчанию отображает Chase-2 и Chase-3 (Chase-1 отключён из-за сложности).

Пример:
  python plot_conv.py --csv combining.csv --out result.png
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
    "chase1_combining": ("Chase-1 + Combining", "tab:blue",   "-",  "o"),
    "chase1_no_comb":   ("Chase-1", "tab:red",    "--", "o"),
}

def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--csv", default="3.csv", help="Входной CSV")
    p.add_argument("--out", help="Выходной PNG (по умолчанию: <csv>.png)")
    p.add_argument("--title", help="Заголовок графика")
    p.add_argument("--log-y", action="store_true", help="Лог-шкала по Y")
    p.add_argument("--chase", nargs="+", type=int, choices=[1,2,3], 
                   default=[2, 3], help="Chase-алгоритмы (по умолчанию: 2 3)")
    p.add_argument("--with-chase1", action="store_true", help="Разрешить отображение Chase-1")
    p.add_argument("--comb-only", action="store_true", help="Только кривые с combining")
    p.add_argument("--no-comb", action="store_true", help="Только кривые без combining")
    return p.parse_args()

def read_csv(path):
    """Читает CSV, пропуская пустые значения."""
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
        sys.exit("❌ Нет данных или отсутствует колонка 'snr_db'")

    snr = [r["snr_db"] for r in rows]

    # Какие колонки рисовать
    allowed = set(args.chase)
    if not args.with_chase1:
        allowed.discard(1)

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
        sys.exit("❌ Нет данных для отображения. Попробуйте --with-chase1 или проверьте CSV")

    # Рисуем
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

    plt.xlabel("SNR (dB)")
    plt.ylabel("Average retransmissions")
    if args.title:
        plt.title(args.title)
    else:
        plt.title("HARQ — Convolutional Codes")
    plt.grid(True, linestyle="--", alpha=0.6)
    plt.legend(fontsize=9)
    if args.log_y:
        plt.yscale("log")
        plt.grid(True, linestyle=":", alpha=0.4, which="minor")

    out = Path(args.out) if args.out else path.with_suffix(".png")
    out.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"✓ Saved: {out}")

if __name__ == "__main__":
    main()