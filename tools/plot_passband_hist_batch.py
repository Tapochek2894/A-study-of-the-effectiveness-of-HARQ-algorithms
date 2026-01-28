#!/usr/bin/env python3
import os
import subprocess
import sys


def main():
    if len(sys.argv) < 3:
        print(
            "Usage: plot_passband_hist_batch.py <bpsk_passband_cloud_bin>"
            " <output_dir> [num_symbols] [carrier_hz] [sample_rate_hz]"
            " [samples_per_symbol] [amplitude] [phase_rad] [seed]",
            file=sys.stderr,
        )
        return 1

    binary_path = sys.argv[1]
    output_dir = sys.argv[2]
    num_symbols = sys.argv[3] if len(sys.argv) > 3 else "20000"
    carrier_hz = sys.argv[4] if len(sys.argv) > 4 else "1000"
    sample_rate_hz = sys.argv[5] if len(sys.argv) > 5 else "10000"
    samples_per_symbol = sys.argv[6] if len(sys.argv) > 6 else "4"
    amplitude = sys.argv[7] if len(sys.argv) > 7 else "1.0"
    phase = sys.argv[8] if len(sys.argv) > 8 else "0.0"
    seed = sys.argv[9] if len(sys.argv) > 9 else "5489"

    os.makedirs(output_dir, exist_ok=True)

    for snr_db in range(0, 11):
        csv_path = os.path.join(output_dir, f"passband_cloud_snr{snr_db}.csv")
        png_path = os.path.join(output_dir, f"passband_hist_snr{snr_db}.png")

        subprocess.run(
            [
                binary_path,
                csv_path,
                num_symbols,
                str(snr_db),
                carrier_hz,
                sample_rate_hz,
                samples_per_symbol,
                amplitude,
                phase,
                seed,
            ],
            check=True,
        )

        subprocess.run(
            [
                sys.executable,
                os.path.join(os.path.dirname(__file__), "plot_passband_hist.py"),
                csv_path,
                png_path,
                "40",
                str(snr_db),
                amplitude,
                samples_per_symbol,
            ],
            check=True,
        )

        print(f"Saved {png_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
