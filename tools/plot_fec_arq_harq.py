#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load_csv(path: Path):
    out = {"snr_db": [], "arq_ber": [], "arq_bler": [], "arq_avg_retx": [],
           "harq_ber": [], "harq_bler": [], "harq_avg_retx": []}
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if not row:
                continue
            for k in out:
                out[k].append(float(row[k]))
    return out


def parse_args():
    p = argparse.ArgumentParser(description="Plot ARQ/HARQ comparison: Hamming vs convolutional.")
    p.add_argument("--hamming-csv", type=Path, required=True)
    p.add_argument("--conv-csv", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--title", default="ARQ/HARQ: Hamming vs Convolutional (AFF3CT)")
    return p.parse_args()


def main():
    args = parse_args()
    h = load_csv(args.hamming_csv)
    c = load_csv(args.conv_csv)

    fig, axes = plt.subplots(1, 3, figsize=(16, 5))

    axes[0].semilogy(h["snr_db"], h["arq_ber"], "b-o", label="ARQ Hamming")
    axes[0].semilogy(h["snr_db"], h["harq_ber"], "b--s", label="HARQ Hamming")
    axes[0].semilogy(c["snr_db"], c["arq_ber"], "r-o", label="ARQ Conv")
    axes[0].semilogy(c["snr_db"], c["harq_ber"], "r--s", label="HARQ Conv")
    axes[0].set_xlabel("SNR (dB)")
    axes[0].set_ylabel("BER")
    axes[0].set_title("BER")
    axes[0].grid(True, which="both", linestyle="--", alpha=0.5)

    axes[1].semilogy(h["snr_db"], h["arq_bler"], "b-o", label="ARQ Hamming")
    axes[1].semilogy(h["snr_db"], h["harq_bler"], "b--s", label="HARQ Hamming")
    axes[1].semilogy(c["snr_db"], c["arq_bler"], "r-o", label="ARQ Conv")
    axes[1].semilogy(c["snr_db"], c["harq_bler"], "r--s", label="HARQ Conv")
    axes[1].set_xlabel("SNR (dB)")
    axes[1].set_ylabel("BLER")
    axes[1].set_title("BLER")
    axes[1].grid(True, which="both", linestyle="--", alpha=0.5)

    axes[2].plot(h["snr_db"], h["arq_avg_retx"], "b-o", label="ARQ Hamming")
    axes[2].plot(h["snr_db"], h["harq_avg_retx"], "b--s", label="HARQ Hamming")
    axes[2].plot(c["snr_db"], c["arq_avg_retx"], "r-o", label="ARQ Conv")
    axes[2].plot(c["snr_db"], c["harq_avg_retx"], "r--s", label="HARQ Conv")
    axes[2].set_xlabel("SNR (dB)")
    axes[2].set_ylabel("Avg retransmissions")
    axes[2].set_title("Average Retransmissions")
    axes[2].grid(True, linestyle="--", alpha=0.5)

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=4)
    fig.suptitle(args.title)
    fig.tight_layout(rect=[0, 0, 1, 0.92])

    args.out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.out, dpi=160)
    print(args.out)


if __name__ == "__main__":
    main()
