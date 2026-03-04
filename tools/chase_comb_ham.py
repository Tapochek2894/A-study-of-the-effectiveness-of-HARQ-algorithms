#!/usr/bin/env python3
"""
Построение графика среднего числа повторных передач HARQ.

Поддерживаемые столбцы CSV (все опциональны, кроме snr_db):
  chase1_combining, chase2_combining, chase3_combining
  chase1_no_comb,   chase2_no_comb,   chase3_no_comb

Примеры запуска:
  python chase_comb_ham.py
  python chase_comb_ham.py --csv results.csv --out metrics/plot.png --title "HARQ (15,11)"
  python chase_comb_ham.py --no-combining --log-y
  python chase_comb_ham.py --chase 1 3          # только методы 1 и 3
"""

import argparse
import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Описание каждого столбца: (label, color, linestyle, marker)
COLUMN_STYLE = {
    "chase1_combining": ("Chase-1 combining", "tab:blue",   "-",  "o"),
    "chase2_combining": ("Chase-2 combining", "tab:cyan",   "-",  "s"),
    "chase3_combining": ("Chase-3 combining", "tab:green",  "-",  "^"),
    "chase1_no_comb":   ("Chase-1 без комб.", "tab:red",    "--", "o"),
    "chase2_no_comb":   ("Chase-2 без комб.", "tab:orange", "--", "s"),
    "chase3_no_comb":   ("Chase-3 без комб.", "tab:brown",  "--", "^"),
}


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv",    default="3.csv",
                   help="Путь к CSV-файлу (default: 3.csv)")
    p.add_argument("--out",    default=None,
                   help="Путь к выходному PNG (default: <csv_stem>_plot.png)")
    p.add_argument("--title",  default=None,
                   help="Заголовок графика")
    p.add_argument("--log-y",  action="store_true",
                   help="Логарифмическая ось Y")
    p.add_argument("--chase",  nargs="+", type=int, choices=[1, 2, 3],
                   default=[1, 2, 3], metavar="N",
                   help="Какие методы Чейза отображать (default: 1 2 3)")
    p.add_argument("--no-combining", action="store_true",
                   help="Показывать только кривые без комбинирования")
    p.add_argument("--combining-only", action="store_true",
                   help="Показывать только кривые с комбинированием")
    return p.parse_args()


def load_csv(path: Path):
    rows = []
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({k: float(v) for k, v in row.items()})
    return rows


def main():
    args = parse_args()
    csv_path = Path(args.csv)
    if not csv_path.exists():
        sys.exit(f"Файл не найден: {csv_path}")

    rows = load_csv(csv_path)
    snr = [r["snr_db"] for r in rows]

    # Определяем, какие столбцы рисовать
    active_methods = set(args.chase)
    columns_to_plot = []
    for col in COLUMN_STYLE:
        method_num = int(col[5])  # 'chase1_...' → 1
        is_combining = "no_comb" not in col

        if method_num not in active_methods:
            continue
        if args.combining_only and not is_combining:
            continue
        if args.no_combining and is_combining:
            continue
        if col in rows[0]:
            columns_to_plot.append(col)

    if not columns_to_plot:
        sys.exit("Нет подходящих столбцов для отображения.")

    fig, ax = plt.subplots(figsize=(9, 6))

    for col in columns_to_plot:
        label, color, ls, marker = COLUMN_STYLE[col]
        values = [row[col] for row in rows]
        ax.plot(snr, values, color=color, linestyle=ls, marker=marker,
                label=label, linewidth=2, markersize=5)

    ax.set_xlabel("SNR (dB)", fontsize=12)
    ax.set_ylabel("Среднее число повторных передач", fontsize=12)

    title = args.title or "Сравнение стратегий HARQ — методы Чейза"
    ax.set_title(title, fontsize=14)
    ax.grid(True, linestyle="--", alpha=0.7)
    ax.legend(fontsize=10)

    if args.log_y:
        ax.set_yscale("log")

    out_path = Path(args.out) if args.out else csv_path.with_name(csv_path.stem + "_plot.png")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"saved: {out_path}")


if __name__ == "__main__":
    main()
