#!/usr/bin/env python3
"""Plot CRC BER/BLER and detection probabilities from CSV produced by crc_ber_bler_sim."""

import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load_csv(path: Path):
    """Load simulation results from CSV file."""
    data = {
        "p": [],
        "ber": [],
        "bler": [],
        "p_detected": [],
        "p_undetected": [],
        "ber_theory": [],
        "bler_theory": [],
        "p_detected_theory": [],
        "p_undetected_theory": [],
    }

    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if not row:
                continue
            for key in data:
                if key in row and row[key] != "":
                    data[key].append(float(row[key]))

    return data


def parse_args():
    parser = argparse.ArgumentParser(
        description="Plot CRC BER/BLER vs p from CSV produced by crc_ber_bler_sim."
    )
    parser.add_argument("csv_path", help="Path to CSV file")
    parser.add_argument(
        "--out",
        help="Output image path (default: <csv_name>_plot.png)",
        default=None,
    )
    parser.add_argument(
        "--log-x",
        action="store_true",
        help="Use logarithmic scale on X axis",
    )
    parser.add_argument(
        "--log-y",
        action="store_true",
        help="Use logarithmic scale on Y axis",
    )
    parser.add_argument(
        "--title",
        default="CRC Error Detection Performance",
        help="Plot title",
    )
    parser.add_argument(
        "--mode",
        choices=["all", "ber_bler", "detection"],
        default="all",
        help="What to plot: all metrics, just BER/BLER, or detection probabilities",
    )
    return parser.parse_args()


def plot_all(ax, data, show_theory=True):
    """Plot all metrics on a single axis."""
    p = data["p"]

    # Simulation results
    ax.plot(p, data["ber"], "o-", label="BER (sim)", color="blue", markersize=4)
    ax.plot(p, data["bler"], "s-", label="BLER (sim)", color="red", markersize=4)
    ax.plot(p, data["p_detected"], "^-", label="P(detected) (sim)", color="green", markersize=4)
    ax.plot(p, data["p_undetected"], "v-", label="P(undetected) (sim)", color="orange", markersize=4)

    # Theory curves (detection only)
    if show_theory and data["p_detected_theory"]:
        ax.plot(p, data["p_detected_theory"], "--", label="P(detected) (theory)", color="green", alpha=0.7)
        ax.plot(p, data["p_undetected_theory"], "--", label="P(undetected) (theory)", color="orange", alpha=0.7)


def plot_ber_bler(ax, data):
    """Plot BER and BLER only."""
    p = data["p"]

    ax.plot(p, data["ber"], "o-", label="BER (sim)", color="blue", markersize=5)
    ax.plot(p, data["bler"], "s-", label="BLER (sim)", color="red", markersize=5)

    # No theory curves for BER/BLER


def plot_detection(ax, data, show_theory=True):
    """Plot detection probabilities only."""
    p = data["p"]

    ax.plot(p, data["p_detected"], "^-", label="P(detected) (sim)", color="green", markersize=5)
    ax.plot(p, data["p_undetected"], "v-", label="P(undetected) (sim)", color="orange", markersize=5)

    if show_theory and data["p_detected_theory"]:
        ax.plot(p, data["p_detected_theory"], "--", label="P(detected) (theory)", color="green", alpha=0.7, linewidth=2)
        ax.plot(p, data["p_undetected_theory"], "--", label="P(undetected) (theory)", color="orange", alpha=0.7, linewidth=2)


def main():
    args = parse_args()
    csv_path = Path(args.csv_path)
    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    data = load_csv(csv_path)

    if not data["p"]:
        raise SystemExit("No data found in CSV file.")

    fig, ax = plt.subplots(figsize=(10, 6))

    show_theory = bool(data["ber_theory"])

    if args.mode == "all":
        plot_all(ax, data, show_theory)
    elif args.mode == "ber_bler":
        plot_ber_bler(ax, data)
    elif args.mode == "detection":
        plot_detection(ax, data, show_theory)

    ax.set_xlabel("Channel Error Probability (p)")
    ax.set_ylabel("Probability")
    ax.set_title(args.title)
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend(loc="best", fontsize=8)

    if args.log_x:
        ax.set_xscale("log")
    if args.log_y:
        ax.set_yscale("log")

    out_path = Path(args.out) if args.out else csv_path.with_name(
        csv_path.stem + "_plot.png"
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"Plot saved to: {out_path}")


if __name__ == "__main__":
    main()
