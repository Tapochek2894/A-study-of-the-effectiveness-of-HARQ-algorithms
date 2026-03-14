#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(path: Path):
    rows = []
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row:
                rows.append(row)
    return rows


def parse_args():
    p = argparse.ArgumentParser(description="Plot Chase(Hamming) curves with convolutional baseline.")
    p.add_argument("--chase-csv", type=Path, required=True)
    p.add_argument("--conv-csv", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--title", default="Chase (Hamming) vs Conv baseline")
    return p.parse_args()


def col(rows, key):
    return [float(r[key]) for r in rows]


def main():
    args = parse_args()
    chase = load(args.chase_csv)
    conv = load(args.conv_csv)

    snr_chase = col(chase, "snr_db")
    snr_conv = col(conv, "snr_db")

    fig, ax = plt.subplots(figsize=(9, 6))
    ax.semilogy(snr_chase, col(chase, "ber_uncoded"), "k--o", label="Uncoded")
    ax.semilogy(snr_chase, col(chase, "ber_coded"), "b-o", label="Hamming hard")
    ax.semilogy(snr_chase, col(chase, "ber_chase1"), "c-s", label="Hamming Chase-1")
    ax.semilogy(snr_chase, col(chase, "ber_chase2"), "g-^", label="Hamming Chase-2")
    ax.semilogy(snr_chase, col(chase, "ber_chase3"), "m-d", label="Hamming Chase-3")
    ax.semilogy(snr_chase, col(chase, "ber_ml"), "y-*", label="Hamming ML")
    ax.semilogy(snr_conv, col(conv, "ber_coded"), "r-x", linewidth=2, label="Conv (AFF3CT)")

    ax.set_xlabel("SNR (dB)")
    ax.set_ylabel("BER")
    ax.set_title(args.title)
    ax.grid(True, which="both", linestyle="--", alpha=0.6)
    ax.legend(fontsize=9)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(args.out, dpi=160)
    print(args.out)


if __name__ == "__main__":
    main()
