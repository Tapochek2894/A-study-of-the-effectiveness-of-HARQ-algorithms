#!/usr/bin/env python3
"""Plot P_undetected vs SNR from CSV produced by crc_undetected_sim."""

import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


COLUMNS_UNC = ("p_undetected_unc", "p_detected_unc", "p_correct_unc")
COLUMNS_COD = ("p_undetected_cod", "p_detected_cod", "p_correct_cod")


def load_csv(path: Path):
    data = {"snr_db": []}
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        fieldnames = reader.fieldnames or []
        for name in fieldnames:
            if name == "snr_db":
                continue
            data[name] = []
        for row in reader:
            if not row:
                continue
            data["snr_db"].append(float(row["snr_db"]))
            for key in data:
                if key == "snr_db":
                    continue
                value = row.get(key, "")
                data[key].append(float(value) if value not in ("", None) else None)
    return data


def parse_args():
    parser = argparse.ArgumentParser(
        description="Plot P_undetected (CRC) vs SNR for uncoded and conv-coded BPSK/Rayleigh."
    )
    parser.add_argument("csv_path", help="Path to CSV file from crc_undetected_sim")
    parser.add_argument(
        "--out",
        help="Output image path (default: <csv_name>_plot.png)",
        default=None,
    )
    parser.add_argument(
        "--title",
        default="P(undetected CRC error) vs SNR, BPSK + Rayleigh",
        help="Plot title",
    )
    parser.add_argument(
        "--with-detected",
        action="store_true",
        help="Also draw P_detected curves (CRC fail rate)",
    )
    parser.add_argument(
        "--floor",
        type=float,
        default=1e-7,
        help="Floor value substituted for zero counts on log scale (default 1e-7)",
    )
    return parser.parse_args()


def _series(data, key, floor):
    raw = data.get(key)
    if raw is None:
        return None
    return [v if (v is not None and v > 0) else floor for v in raw]


def main():
    args = parse_args()
    csv_path = Path(args.csv_path)
    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    data = load_csv(csv_path)
    if not data["snr_db"]:
        raise SystemExit("No data found in CSV file.")

    snr = data["snr_db"]
    fig, ax = plt.subplots(figsize=(10, 6))

    p_und_unc = _series(data, "p_undetected_unc", args.floor)
    p_und_cod = _series(data, "p_undetected_cod", args.floor)
    if p_und_unc is not None:
        ax.semilogy(snr, p_und_unc, "o-", color="tab:red",
                    label="P_undetected, без кодирования", markersize=5)
    if p_und_cod is not None:
        ax.semilogy(snr, p_und_cod, "s-", color="tab:blue",
                    label="P_undetected, conv 1/2 Viterbi", markersize=5)

    if args.with_detected:
        p_det_unc = _series(data, "p_detected_unc", args.floor)
        p_det_cod = _series(data, "p_detected_cod", args.floor)
        if p_det_unc is not None:
            ax.semilogy(snr, p_det_unc, "v--", color="tab:orange", alpha=0.7,
                        label="P_detected, без кодирования", markersize=4)
        if p_det_cod is not None:
            ax.semilogy(snr, p_det_cod, "^--", color="tab:cyan", alpha=0.7,
                        label="P_detected, conv 1/2 Viterbi", markersize=4)

    ax.set_xlabel("SNR, dB")
    ax.set_ylabel("Probability")
    ax.set_title(args.title)
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend(loc="best", fontsize=9)

    out_path = Path(args.out) if args.out else csv_path.with_name(
        csv_path.stem + "_plot.png"
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"Plot saved to: {out_path}")


if __name__ == "__main__":
    main()
