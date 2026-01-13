#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load_csv(path: Path):
    snr_vals = []
    ber_vals = []
    ber_uncoded = []
    ber_coded = []
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        fieldnames = reader.fieldnames or []
        for row in reader:
            if not row:
                continue
            snr_vals.append(float(row["snr_db"]))
            if "ber" in fieldnames:
                ber_vals.append(float(row["ber"]))
            if "ber_uncoded" in fieldnames:
                ber_uncoded.append(float(row["ber_uncoded"]))
            if "ber_coded" in fieldnames:
                ber_coded.append(float(row["ber_coded"]))
    return snr_vals, ber_vals, ber_uncoded, ber_coded


def parse_args():
    parser = argparse.ArgumentParser(
        description="Plot BER vs SNR (dB) from CSV produced by bpsk_awgn_sim."
    )
    parser.add_argument("csv_path", help="Path to CSV file")
    parser.add_argument(
        "--out",
        help="Output image path (default: <csv_name>_plot.png)",
        default=None,
    )
    parser.add_argument(
        "--log-y",
        action="store_true",
        help="Use logarithmic scale on Y axis",
    )
    parser.add_argument(
        "--title",
        default="BER vs SNR (BPSK AWGN)",
        help="Plot title",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    csv_path = Path(args.csv_path)
    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    snr_vals, ber_vals, ber_uncoded, ber_coded = load_csv(csv_path)

    fig, ax = plt.subplots()
    if ber_vals:
        ax.plot(snr_vals, ber_vals, marker="o", label="BER")
    if ber_uncoded:
        ax.plot(snr_vals, ber_uncoded, marker="o", label="BER uncoded")
    if ber_coded:
        ax.plot(snr_vals, ber_coded, marker="s", label="BER coded")
    ax.set_xlabel("SNR (dB)")
    ax.set_ylabel("BER")
    ax.set_title(args.title)
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend()

    if args.log_y:
        ax.set_yscale("log")

    out_path = Path(args.out) if args.out else csv_path.with_name(
        csv_path.stem + "_plot.png"
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(out_path)


if __name__ == "__main__":
    main()
