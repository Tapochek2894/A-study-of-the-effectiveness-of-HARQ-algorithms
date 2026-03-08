#!/usr/bin/env python3
"""
Plot BER vs SNR for QPSK simulation with optional theoretical curve.
Compatible with CSV output from qpsk_awgn_sim.cpp
"""
import argparse
import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")  # Non-interactive backend for saving figures
import matplotlib.pyplot as plt
import numpy as np

# --- FIX: Import erfc from scipy.special ---
try:
    from scipy.special import erfc
except ImportError:
    print("❌ Error: 'scipy' is required for this script.")
    print("   Install it via: pip install scipy")
    sys.exit(1)
# -------------------------------------------


def q_function(x):
    """
    Q-function: Q(x) = 0.5 * erfc(x / sqrt(2))
    
    Parameters
    ----------
    x : float or ndarray
    
    Returns
    -------
    float or ndarray
    """
    x = np.asarray(x)
    return 0.5 * erfc(x / np.sqrt(2.0))


def ber_qpsk_theory(eb_n0_db):
    """
    Theoretical BER for Gray-coded QPSK in AWGN.
    P_b = Q(sqrt(2 * Eb/N0))
    """
    eb_n0_linear = 10 ** (np.asarray(eb_n0_db) / 10.0)
    return q_function(np.sqrt(2.0 * eb_n0_linear) / np.sqrt(2))


def ber_qpsk_symbol_error_theory(es_n0_db):
    """
    Theoretical Symbol Error Rate (SER) for QPSK in AWGN.
    SER = 2*Q(sqrt(Es/N0)) - Q^2(sqrt(Es/N0))
    """
    es_n0_linear = 10 ** (np.asarray(es_n0_db) / 10.0)
    q_val = q_function(np.sqrt(es_n0_linear))
    return 2 * q_val - q_val ** 2


def load_csv(path: Path):
    """Load BER data from CSV file produced by qpsk_awgn_sim."""
    snr_vals = []
    ber_vals = []
    ber_uncoded = []
    ber_coded = []
    
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        fieldnames = reader.fieldnames or []
        
        for row in reader:
            if not row or "snr_db" not in row:
                continue
            try:
                snr_vals.append(float(row["snr_db"]))
            except (ValueError, KeyError):
                continue
                
            if "ber" in fieldnames and row.get("ber"):
                ber_vals.append(float(row["ber"]))
            if "ber_uncoded" in fieldnames and row.get("ber_uncoded"):
                ber_uncoded.append(float(row["ber_uncoded"]))
            if "ber_coded" in fieldnames and row.get("ber_coded"):
                ber_coded.append(float(row["ber_coded"]))
    
    return snr_vals, ber_vals, ber_uncoded, ber_coded


def parse_args():
    parser = argparse.ArgumentParser(
        description="Plot BER vs SNR (dB) from CSV produced by qpsk_awgn_sim."
    )
    parser.add_argument("csv_path", type=Path, help="Path to input CSV file")
    parser.add_argument(
        "--out", "-o",
        type=Path,
        default=None,
        help="Output image path (default: <csv_name>_qpsk_plot.png)"
    )
    parser.add_argument(
        "--log-y",
        action="store_true",
        help="Use logarithmic scale on Y axis (recommended for BER)"
    )
    parser.add_argument(
        "--title",
        default=None,
        help="Plot title (default: auto-generated)"
    )
    parser.add_argument(
        "--theory",
        action="store_true",
        help="Add theoretical BER curve for Gray-coded QPSK"
    )
    parser.add_argument(
        "--theory-ser",
        action="store_true",
        help="Also plot theoretical Symbol Error Rate (SER) curve"
    )
    parser.add_argument(
        "--ebn0",
        action="store_true",
        help="Label x-axis as Eb/N0 (dB) instead of SNR (dB)"
    )
    parser.add_argument(
        "--xlim",
        type=float,
        nargs=2,
        metavar=("MIN", "MAX"),
        help="Set x-axis limits: --xlim 0 12"
    )
    parser.add_argument(
        "--ylim",
        type=float,
        nargs=2,
        metavar=("MIN", "MAX"),
        help="Set y-axis limits: --ylim 1e-5 1"
    )
    parser.add_argument(
        "--no-legend",
        action="store_true",
        help="Hide legend"
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--coded",
        action="store_true",
        help="Plot coded BER only (when coded columns are present)"
    )
    mode.add_argument(
        "--uncoded",
        action="store_true",
        help="Plot uncoded BER only"
    )
    return parser.parse_args()


def main():
    args = parse_args()
    csv_path = args.csv_path
    
    if not csv_path.exists():
        raise SystemExit(f"❌ CSV file not found: {csv_path}")

    snr_vals, ber_vals, ber_uncoded, ber_coded = load_csv(csv_path)
    
    if not snr_vals:
        raise SystemExit(f"❌ No valid data found in {csv_path}")

    has_uncoded = bool(ber_uncoded and len(ber_uncoded) == len(snr_vals))
    has_coded = bool(ber_coded and len(ber_coded) == len(snr_vals))
    has_simple_ber = bool(ber_vals and len(ber_vals) == len(snr_vals))

    if args.coded:
        has_uncoded = False
        has_simple_ber = False
    if args.uncoded:
        has_coded = False

    fig, ax = plt.subplots(figsize=(8, 5), dpi=100)
    
    if has_simple_ber:
        ax.plot(snr_vals, ber_vals, 'bo-', markersize=4, 
                label='BER (simulation)', linewidth=1.5)
    
    if has_uncoded:
        ax.plot(snr_vals, ber_uncoded, 'bo-', markersize=4, 
                label='BER uncoded (QPSK)', linewidth=1.5)
    
    if has_coded:
        ax.plot(snr_vals, ber_coded, 'rs--', markersize=5,
                label='BER coded (FEC)', linewidth=1.5, markevery=2)

    if args.theory:
        snr_min, snr_max = min(snr_vals), max(snr_vals)
        snr_theory = np.linspace(snr_min, snr_max, 300)
        ber_theory = ber_qpsk_theory(snr_theory)
        ax.plot(snr_theory, ber_theory, 'k:', linewidth=2, 
                label='Theory: QPSK $P_b=Q(\\sqrt{2E_b/N_0})$')
    
    if args.theory_ser and has_uncoded:
        snr_min, snr_max = min(snr_vals), max(snr_vals)
        snr_theory = np.linspace(snr_min, snr_max, 300)
        # Es/N0 = Eb/N0 + 3 dB for QPSK
        es_n0_theory = snr_theory + 10 * np.log10(2)  
        ser_theory = ber_qpsk_symbol_error_theory(es_n0_theory)
        ax.plot(snr_theory, ser_theory, 'm-.', linewidth=1.5, 
                label='Theory: QPSK SER', alpha=0.8)

    xlabel = r"$E_b/N_0$ (dB)" if args.ebn0 else "SNR (dB)"
    ax.set_xlabel(xlabel, fontsize=11)
    ax.set_ylabel("Bit Error Rate (BER)", fontsize=11)
    
    title = args.title
    if title is None:
        if has_coded:
            title = "QPSK BER: Uncoded vs. Coded (FEC)"
        else:
            title = "QPSK Bit Error Rate in AWGN"
    ax.set_title(title, fontsize=13, pad=15)
    
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.7)
    ax.set_axisbelow(True)
    
    if args.log_y:
        ax.set_yscale("log")
        ax.set_ylim(bottom=1e-6)
    
    if args.xlim:
        ax.set_xlim(args.xlim)
    if args.ylim:
        ax.set_ylim(args.ylim)
    
    if not args.no_legend:
        ax.legend(loc="best", fontsize=9, framealpha=0.9)
    
    out_path = args.out
    if out_path is None:
        out_path = csv_path.with_name(csv_path.stem + "_qpsk_plot.png")
    
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"✓ Plot saved: {out_path}")


if __name__ == "__main__":
    main()
