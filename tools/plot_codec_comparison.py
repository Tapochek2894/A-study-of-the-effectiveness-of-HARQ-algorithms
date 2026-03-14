#!/usr/bin/env python3
import argparse
import csv
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def q_function(x):
    x = np.asarray(x)
    return np.vectorize(lambda v: 0.5 * math.erfc(v / math.sqrt(2.0)))(x)


def ber_theory_bpsk_qpsk(snr_db):
    snr_linear = 10 ** (np.asarray(snr_db) / 10.0)
    return q_function(np.sqrt(snr_linear))


def read_csv(path: Path):
    rows = []
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row:
                rows.append(row)
    if not rows:
        raise ValueError(f"No rows in CSV: {path}")
    return rows


def extract_series(rows, snr_key, uncoded_key, coded_key):
    snr = []
    uncoded = []
    coded = []
    for r in rows:
        snr.append(float(r[snr_key]))
        uncoded.append(float(r[uncoded_key]))
        coded.append(float(r[coded_key]))
    return np.array(snr), np.array(uncoded), np.array(coded)


def parse_args():
    p = argparse.ArgumentParser(
        description="Plot codec comparison (Hamming vs convolutional AFF3CT)."
    )
    p.add_argument("--hamming-csv", type=Path, required=True)
    p.add_argument("--conv-csv", type=Path, required=True)
    p.add_argument(
        "--kind",
        choices=["bpsk_awgn", "qpsk_awgn", "qpsk_bpsk_bpsk", "qpsk_bpsk_qpsk"],
        required=True,
    )
    p.add_argument("--title", default=None)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--theory", action="store_true")
    p.add_argument("--log-y", action="store_true", default=True)
    return p.parse_args()


def main():
    args = parse_args()
    h_rows = read_csv(args.hamming_csv)
    c_rows = read_csv(args.conv_csv)

    if args.kind == "bpsk_awgn":
        snr_key = "snr_db"
        uncoded_key = "ber_uncoded"
        coded_key = "ber_coded"
        default_title = "BPSK: Hamming vs Convolutional (AFF3CT)"
    elif args.kind == "qpsk_awgn":
        snr_key = "snr_db"
        uncoded_key = "ber_uncoded"
        coded_key = "ber_coded"
        default_title = "QPSK: Hamming vs Convolutional (AFF3CT)"
    elif args.kind == "qpsk_bpsk_bpsk":
        snr_key = "snr_db"
        uncoded_key = "bpsk_ber_uncoded"
        coded_key = "bpsk_ber_coded"
        default_title = "BPSK (combined sim): Hamming vs Convolutional (AFF3CT)"
    else:
        snr_key = "snr_db"
        uncoded_key = "qpsk_ber_uncoded"
        coded_key = "qpsk_ber_coded"
        default_title = "QPSK (combined sim): Hamming vs Convolutional (AFF3CT)"

    h_snr, h_uncoded, h_coded = extract_series(h_rows, snr_key, uncoded_key, coded_key)
    c_snr, c_uncoded, c_coded = extract_series(c_rows, snr_key, uncoded_key, coded_key)

    if h_snr.shape != c_snr.shape or np.max(np.abs(h_snr - c_snr)) > 1e-9:
        raise ValueError("SNR grids differ between Hamming and convolutional CSVs.")

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(h_snr, h_uncoded, "k--", marker="o", label="Uncoded")
    ax.plot(h_snr, h_coded, "b-", marker="s", label="Hamming coded")
    ax.plot(c_snr, c_coded, "r-", marker="^", label="Convolutional coded (AFF3CT)")

    if args.theory:
        snr_theory = np.linspace(float(np.min(h_snr)), float(np.max(h_snr)), 300)
        ax.plot(snr_theory, ber_theory_bpsk_qpsk(snr_theory), "g:", linewidth=2, label="Theory")

    ax.set_xlabel("SNR (dB)")
    ax.set_ylabel("BER")
    ax.set_title(args.title if args.title else default_title)
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend()
    ax.set_yscale("log")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(args.out, dpi=160)
    print(args.out)


if __name__ == "__main__":
    main()
