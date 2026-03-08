#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
from scipy.special import erfc

def q_func(x):
    return 0.5 * erfc(x / np.sqrt(2.0))

def ber_theory(snr_db):
    snr_lin = 10 ** (np.asarray(snr_db) / 10.0)
    return q_func(np.sqrt(2.0 * snr_lin) / np.sqrt(2.0))

def main():
    parser = argparse.ArgumentParser(
        description="Plot BPSK vs QPSK BER from qpsk_bpsk_sim CSV."
    )
    parser.add_argument("csv_path", type=Path, help="Path to input CSV")
    parser.add_argument("--out", "-o", type=Path, default=None,
                        help="Output PNG path")
    parser.add_argument("--theory", action="store_true",
                        help="Overlay theoretical uncoded BER")
    parser.add_argument("--coded", action="store_true",
                        help="Use coded BER columns when available")
    args = parser.parse_args()

    snr_vals = []
    bpsk_ber = []
    qpsk_ber = []
    
    with args.csv_path.open() as f:
        reader = csv.DictReader(f)
        fieldnames = reader.fieldnames or []
        
        for row in reader:
            snr_vals.append(float(row["snr_db"]))
            
            # Choose columns based on requested mode and CSV schema.
            if args.coded and "bpsk_ber_coded" in fieldnames:
                bpsk_ber.append(float(row["bpsk_ber_coded"]))
            elif "bpsk_ber_uncoded" in fieldnames:
                bpsk_ber.append(float(row["bpsk_ber_uncoded"]))
            elif "bpsk_ber" in fieldnames:
                bpsk_ber.append(float(row["bpsk_ber"]))

            if args.coded and "qpsk_ber_coded" in fieldnames:
                qpsk_ber.append(float(row["qpsk_ber_coded"]))
            elif "qpsk_ber_uncoded" in fieldnames:
                qpsk_ber.append(float(row["qpsk_ber_uncoded"]))
            elif "qpsk_ber" in fieldnames:
                qpsk_ber.append(float(row["qpsk_ber"]))

    if not snr_vals:
        print("Error: No data found in CSV")
        return

    fig, ax = plt.subplots(figsize=(8, 6))
    
    mode_label = "coded" if args.coded else "uncoded"
    if bpsk_ber:
        ax.plot(snr_vals, bpsk_ber, 'bo-',
                label=f'BPSK ({mode_label})', markersize=4, linewidth=1.5)
    if qpsk_ber:
        ax.plot(snr_vals, qpsk_ber, 'rs--',
                label=f'QPSK ({mode_label})', markersize=4, linewidth=1.5)
    
    if args.theory and snr_vals:
        s = np.linspace(min(snr_vals), max(snr_vals), 200)
        ax.plot(s, ber_theory(s), 'k:', label='Theory (BPSK/QPSK)', linewidth=2)

    ax.set_xlabel("SNR (dB)")
    ax.set_ylabel("Bit Error Rate")
    ax.set_title(f"Сравнение BPSK и QPSK ({mode_label})")
    ax.set_yscale("log")
    ax.grid(True, which="both", linestyle="--", alpha=0.6)
    ax.legend()
    
    out = args.out or args.csv_path.with_name(args.csv_path.stem + "_comparison.png")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    print(f"Saved: {out}")

if __name__ == "__main__":
    main()
