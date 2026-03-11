#!/usr/bin/env python3
"""Plot ARQ + Chase simulation results.

Usage examples:
  # Сравнить несколько CSV на одном графике:
  python3 tools/plot_arq_chase.py \
      perfect.csv "Perfect detection" \
      crc16.csv   "CRC-16" \
      crc4.csv    "CRC-4 (bad)" \
      --out arq_comparison.png

  # Один файл:
  python3 tools/plot_arq_chase.py results.csv "CRC-8" --out out.png
"""

import argparse
import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load_csv(path: Path) -> dict:
    data = {k: [] for k in
            ("snr_db", "ber", "bler", "avg_retx")}
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if not row:
                continue
            for key in data:
                if key in row and row[key] != "":
                    data[key].append(float(row[key]))
    return data


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot ARQ Chase simulation results from CSV files."
    )
    parser.add_argument(
        "files",
        nargs="+",
        metavar="CSV [LABEL] ...",
        help="Pairs of: <csv_path> <label>. "
             "If only one file is given, label defaults to filename.",
    )
    parser.add_argument("--out", default="arq_chase_plot.png",
                        help="Output image path (default: arq_chase_plot.png)")
    parser.add_argument("--title", default="ARQ + Chase (Hamming)",
                        help="Plot title")
    parser.add_argument("--no-ber", action="store_true",
                        help="Skip BER subplot")
    parser.add_argument("--no-bler", action="store_true",
                        help="Skip BLER subplot")
    parser.add_argument("--no-retx", action="store_true",
                        help="Skip avg retransmissions subplot")
    return parser.parse_args()


def parse_file_label_pairs(args_files: list) -> list:
    """Parse interleaved [csv, label, csv, label, ...] or just [csv, ...]."""
    pairs = []
    i = 0
    while i < len(args_files):
        csv_path = Path(args_files[i])
        i += 1
        if i < len(args_files) and not args_files[i].endswith(".csv"):
            label = args_files[i]
            i += 1
        else:
            label = csv_path.stem
        pairs.append((csv_path, label))
    return pairs


def main() -> None:
    args = parse_args()
    pairs = parse_file_label_pairs(args.files)

    if not pairs:
        print("No input files.", file=sys.stderr)
        sys.exit(1)

    # Определяем, сколько subplot'ов нужно
    panels = []
    if not args.no_ber:
        panels.append(("ber", "BER", "Bit Error Rate"))
    if not args.no_bler:
        panels.append(("bler", "BLER", "Block Error Rate"))
    if not args.no_retx:
        panels.append(("avg_retx", "Avg retx", "Average retransmissions"))

    if not panels:
        print("All panels disabled.", file=sys.stderr)
        sys.exit(1)

    fig, axes = plt.subplots(1, len(panels), figsize=(9 * len(panels), 6))
    if len(panels) == 1:
        axes = [axes]

    markers = ["o", "s", "^", "D", "v", "P", "*", "X"]

    for idx, (csv_path, label) in enumerate(pairs):
        data = load_csv(csv_path)
        snr = data["snr_db"]
        marker = markers[idx % len(markers)]

        for ax, (key, ylabel, _) in zip(axes, panels):
            values = data[key]
            if not values:
                continue
            ax.semilogy(snr, values, marker=marker, label=label,
                        linewidth=1.5, markersize=5)

    for ax, (_, ylabel, title) in zip(axes, panels):
        ax.set_xlabel("SNR, дБ")
        ax.set_ylabel(ylabel)
        ax.set_title(title)
        ax.legend(fontsize=8)
        ax.grid(True, which="both", linestyle="--", alpha=0.5)

    fig.suptitle(args.title)
    fig.tight_layout()
    fig.savefig(args.out, dpi=150)
    print(f"Saved: {args.out}")


if __name__ == "__main__":
    main()
