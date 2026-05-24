#!/usr/bin/env python3
"""Plot CRC error probability vs SNR from the long-format CSV of crc_undetected_sim.

CSV columns: crc,r,mode,snr_db,p_undetected,p_detected,p_correct,
             n_undetected,n_detected,n_correct,total_blocks

По умолчанию строится P_undetected (вероятность НЕ обнаружить ошибку) для всех
CRC и режимов на одном графике. Несколько CRC сравниваются цветом, режимы
(uncoded/coded) — стилем линии.
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


CRC_COLORS = {
    "8": "tab:red",
    "16": "tab:green",
    "24a": "tab:blue",
}
MODE_STYLE = {
    "uncoded": dict(linestyle="--", marker="v"),
    "coded": dict(linestyle="-", marker="o"),
}


def load_csv(path: Path):
    # groups[(crc, mode)] -> list of (snr_db, value, r)
    rows = []
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if not row or "crc" not in row:
                continue
            rows.append(row)
    return rows


def parse_args():
    parser = argparse.ArgumentParser(
        description="Plot CRC undetected/detected probability vs SNR "
        "(long-format CSV from crc_undetected_sim)."
    )
    parser.add_argument("csv_path", help="Path to CSV file from crc_undetected_sim")
    parser.add_argument("--out", default=None, help="Output image path")
    parser.add_argument("--title", default=None, help="Plot title")
    parser.add_argument(
        "--metric",
        choices=("undetected", "detected", "correct"),
        default="undetected",
        help="Which probability to plot (default undetected).",
    )
    parser.add_argument(
        "--modes",
        default="uncoded,coded",
        help="Comma list of modes to draw (default 'uncoded,coded').",
    )
    parser.add_argument(
        "--floor",
        type=float,
        default=1e-9,
        help="Floor for zero values on log scale / lower y-limit (default 1e-9).",
    )
    parser.add_argument(
        "--no-ceilings",
        action="store_true",
        help="Do not draw 2^-r CRC undetected ceilings.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    csv_path = Path(args.csv_path)
    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    rows = load_csv(csv_path)
    if not rows:
        raise SystemExit("No data found in CSV file.")

    wanted_modes = [m for m in args.modes.split(",") if m]
    col = f"p_{args.metric}"

    # groups[(crc, mode)] -> dict(snr -> value), and crc -> r
    groups = defaultdict(dict)
    crc_r = {}
    crc_order = []
    for row in rows:
        crc = row["crc"]
        mode = row["mode"]
        if mode not in wanted_modes:
            continue
        if crc not in crc_r:
            crc_r[crc] = int(row["r"])
            crc_order.append(crc)
        groups[(crc, mode)][float(row["snr_db"])] = float(row[col])

    if not groups:
        raise SystemExit("No matching (crc, mode) series to plot.")

    fig, ax = plt.subplots(figsize=(10, 6))

    for crc in crc_order:
        color = CRC_COLORS.get(crc, None)
        for mode in wanted_modes:
            series = groups.get((crc, mode))
            if not series:
                continue
            snr = sorted(series)
            vals = [series[s] if series[s] > 0 else args.floor for s in snr]
            style = MODE_STYLE.get(mode, dict(linestyle="-", marker="o"))
            ax.semilogy(
                snr, vals, color=color, markersize=5,
                label=f"CRC-{crc.upper()}, {mode}", **style,
            )

    # Горизонтальные ориентиры 2^-r (потолок необнаружения каждого CRC).
    if args.metric == "undetected" and not args.no_ceilings:
        for crc in crc_order:
            ceil = 2.0 ** (-crc_r[crc])
            if ceil >= args.floor:
                ax.axhline(ceil, color=CRC_COLORS.get(crc, "gray"),
                           linestyle=":", linewidth=0.9, alpha=0.5)
                ax.text(ax.get_xlim()[1], ceil, f" 2^-{crc_r[crc]}",
                        va="center", ha="left", fontsize=7,
                        color=CRC_COLORS.get(crc, "gray"))

    titles = {
        "undetected": "Вероятность НЕ обнаружить ошибку (P_undetected) vs SNR\n"
                      "BPSK + Rayleigh, conv 1/3 Viterbi, k=512",
        "detected": "P(CRC detected) vs SNR, BPSK + Rayleigh, k=512",
        "correct": "P(correct) vs SNR, BPSK + Rayleigh, k=512",
    }
    ax.set_xlabel("SNR, dB")
    ax.set_ylabel(f"P_{args.metric}")
    ax.set_title(args.title or titles[args.metric])
    ax.set_ylim(bottom=args.floor)
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend(loc="best", fontsize=9)

    out_path = Path(args.out) if args.out else csv_path.with_name(
        csv_path.stem + f"_{args.metric}.png"
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"Plot saved to: {out_path}")


if __name__ == "__main__":
    main()
