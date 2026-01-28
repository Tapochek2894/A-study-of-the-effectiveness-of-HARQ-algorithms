#!/usr/bin/env python3
import csv
import math
import sys

import matplotlib.pyplot as plt


def read_i_by_bit(csv_path):
    bit0 = []
    bit1 = []
    with open(csv_path, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            bit = row.get("bit")
            i_val = row.get("i")
            if bit is None or i_val is None:
                continue
            try:
                bit_int = int(bit)
                i_float = float(i_val)
            except ValueError:
                continue
            if bit_int == 0:
                bit0.append(i_float)
            elif bit_int == 1:
                bit1.append(i_float)
    return bit0, bit1


def main():
    if len(sys.argv) < 3:
        print(
            "Usage: plot_passband_hist.py <input_csv> <output_png> [bins]"
            " [snr_db amplitude samples_per_symbol]",
            file=sys.stderr,
        )
        return 1

    csv_path = sys.argv[1]
    output_path = sys.argv[2]
    bins = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    snr_db = float(sys.argv[4]) if len(sys.argv) > 4 else None
    amplitude = float(sys.argv[5]) if len(sys.argv) > 5 else None
    samples_per_symbol = int(sys.argv[6]) if len(sys.argv) > 6 else None

    bit0, bit1 = read_i_by_bit(csv_path)
    if not bit0 and not bit1:
        print("No data found in CSV.", file=sys.stderr)
        return 1

    plt.figure(figsize=(8, 3.5))
    plt.hist(bit1, bins=bins, density=True, alpha=0.7, label="Бит 1", color="#1f77b4")
    plt.hist(bit0, bins=bins, density=True, alpha=0.7, label="Бит 0", color="#d62728")

    if snr_db is not None and amplitude is not None and samples_per_symbol is not None:
        if samples_per_symbol <= 0:
            print("samples_per_symbol must be positive.", file=sys.stderr)
            return 1
        snr_linear = 10 ** (snr_db / 10.0)
        sigma2 = (amplitude * amplitude * 0.5) / snr_linear
        sigma_i = math.sqrt((2.0 * sigma2) / samples_per_symbol)
        x_min = min(min(bit0, default=0.0), min(bit1, default=0.0))
        x_max = max(max(bit0, default=0.0), max(bit1, default=0.0))
        pad = 3.0 * sigma_i
        x_min -= pad
        x_max += pad
        steps = 600
        xs = [x_min + (x_max - x_min) * i / (steps - 1) for i in range(steps)]

        def gaussian_pdf(x, mu, sigma):
            coef = 1.0 / (sigma * math.sqrt(2.0 * math.pi))
            exp_val = math.exp(-0.5 * ((x - mu) / sigma) ** 2)
            return coef * exp_val

        pdf1 = [gaussian_pdf(x, amplitude, sigma_i) for x in xs]
        pdf0 = [gaussian_pdf(x, -amplitude, sigma_i) for x in xs]
        plt.plot(
            xs,
            pdf1,
            color="#1f77b4",
            linewidth=2.0,
            linestyle="--",
            label="Теория бит 1",
        )
        plt.plot(
            xs,
            pdf0,
            color="#d62728",
            linewidth=2.0,
            linestyle="--",
            label="Теория бит 0",
        )

    plt.title("Распределение")
    plt.xlabel("Амплитуда")
    plt.ylabel("Плотность")
    plt.grid(True, alpha=0.2)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
