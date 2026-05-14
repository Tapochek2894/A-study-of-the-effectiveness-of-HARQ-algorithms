#!/usr/bin/env python3
"""Построение BER vs SNR для CSV, полученного из bpsk_rayleigh_sim / qpsk_rayleigh_sim.

Теоретические кривые:
- BPSK/QPSK AWGN: P_e = Q(sqrt(2·γ)).
- BPSK/QPSK Rayleigh (когерентный, flat-fading, E[μ²]=1):
    P_e = 0.5·(1 − sqrt(γ̄/(1+γ̄))),
  где γ̄ = 10^(SNR_dB/10) — средний SNR на бит.
  (Трофимов, §3.8; Скляр «Цифровая связь», гл. 14.)
"""
import argparse
import csv
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def q_function(x):
    x = np.asarray(x, dtype=float)
    return np.vectorize(lambda v: 0.5 * math.erfc(v / math.sqrt(2)))(x)


def ber_awgn_theory(snr_db):
    snr = 10 ** (np.asarray(snr_db, dtype=float) / 10.0)
    return q_function(np.sqrt(snr))


def ber_rayleigh_theory(snr_db):
    g = 10 ** (np.asarray(snr_db, dtype=float) / 10.0)
    return 0.5 * (1.0 - np.sqrt(g / (1.0 + g)))


def load_csv(path):
    snr, ber, ber_u, ber_c = [], [], [], []
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        names = reader.fieldnames or []
        for row in reader:
            if not row:
                continue
            snr.append(float(row["snr_db"]))
            if "ber" in names:
                ber.append(float(row["ber"]))
            if "ber_uncoded" in names:
                ber_u.append(float(row["ber_uncoded"]))
            if "ber_coded" in names:
                ber_c.append(float(row["ber_coded"]))
    return snr, ber, ber_u, ber_c


def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("csv_path", help="CSV из bpsk_rayleigh_sim / qpsk_rayleigh_sim")
    p.add_argument("--out", default=None,
                   help="Куда сохранить PNG (по умолчанию <csv>_plot.png)")
    p.add_argument("--title", default="BER vs SNR (Rayleigh)")
    p.add_argument("--with-awgn-theory", action="store_true",
                   help="Добавить теоретическую кривую AWGN для сравнения")
    p.add_argument("--linear-y", action="store_true",
                   help="Линейная ось Y (по умолчанию log)")
    return p.parse_args()


def main():
    args = parse_args()
    csv_path = Path(args.csv_path)
    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    snr, ber, ber_u, ber_c = load_csv(csv_path)
    if not snr:
        raise SystemExit("CSV пустой")

    fig, ax = plt.subplots(figsize=(7.5, 5.5))
    if ber:
        ax.plot(snr, ber, marker="o", linewidth=1.5,
                label="BER симуляция (uncoded)")
    if ber_u:
        ax.plot(snr, ber_u, marker="o", linewidth=1.5, label="BER uncoded")
    if ber_c:
        ax.plot(snr, ber_c, marker="s", linewidth=1.5, label="BER coded")

    snr_grid = np.linspace(min(snr), max(snr), 200)
    ax.plot(snr_grid, ber_rayleigh_theory(snr_grid), "k--", linewidth=1.2,
            label="теория Rayleigh")
    if args.with_awgn_theory:
        ax.plot(snr_grid, ber_awgn_theory(snr_grid), "r:", linewidth=1.2,
                label="теория AWGN (для сравнения)")

    ax.set_xlabel("SNR, дБ")
    ax.set_ylabel("BER")
    ax.set_title(args.title)
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend()
    if not args.linear_y:
        ax.set_yscale("log")
        ax.set_ylim(bottom=max(1e-6, min(filter(lambda x: x > 0,
                                                ber + ber_u + ber_c
                                                + [1e-6])) / 5))

    out_path = Path(args.out) if args.out else csv_path.with_name(
        csv_path.stem + "_plot.png")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(out_path)


if __name__ == "__main__":
    main()
