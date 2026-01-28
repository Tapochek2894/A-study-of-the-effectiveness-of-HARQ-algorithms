#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(
        description="Generate scatter clouds for multiple SNR values."
    )
    parser.add_argument("binary_path", help="Path to bpsk_passband_cloud binary")
    parser.add_argument("output_dir", help="Directory for CSV/PNG outputs")
    parser.add_argument(
        "--snr",
        type=float,
        nargs="+",
        default=[0.0, 5.0, 10.0],
        help="SNR values in dB",
    )
    parser.add_argument("--num-symbols", default="20000")
    parser.add_argument("--carrier-hz", default="1000")
    parser.add_argument("--sample-rate-hz", default="10000")
    parser.add_argument("--samples-per-symbol", default="4")
    parser.add_argument("--amplitude", default="1.0")
    parser.add_argument("--phase", default="0.0")
    parser.add_argument("--seed", default="5489")
    parser.add_argument("--limit", default="2.0")
    parser.add_argument("--point-size", default="10.0")
    parser.add_argument("--alpha", default="0.35")
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    scatter_script = os.path.join(os.path.dirname(__file__), "plot_passband_scatter.py")

    for snr_db in args.snr:
        snr_str = f"{snr_db:g}"
        csv_path = os.path.join(args.output_dir, f"passband_cloud_snr{snr_str}.csv")
        png_path = os.path.join(args.output_dir, f"passband_scatter_snr{snr_str}.png")
        title = f"Облака рассеивания (SNR = {snr_str} dB)"

        subprocess.run(
            [
                args.binary_path,
                csv_path,
                args.num_symbols,
                snr_str,
                args.carrier_hz,
                args.sample_rate_hz,
                args.samples_per_symbol,
                args.amplitude,
                args.phase,
                args.seed,
            ],
            check=True,
        )

        subprocess.run(
            [
                sys.executable,
                scatter_script,
                csv_path,
                png_path,
                "--limit",
                args.limit,
                "--point-size",
                args.point_size,
                "--alpha",
                args.alpha,
                "--title",
                title,
            ],
            check=True,
        )

        print(f"Saved {png_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
