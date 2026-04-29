import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


def load_data(filename):
    data = {}

    with open(filename, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            for name, value in row.items():
                data.setdefault(name, []).append(float(value))

    return data


def save_current_figure(output_dir, name):
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / f"{name}.png"
    plt.tight_layout()
    plt.savefig(path, dpi=180)
    return path


def plot_metric(snr, series, semilogy=False):
    has_positive_values = any(any(value > 0.0 for value in values) for values, _, _ in series)
    plot = plt.semilogy if semilogy and has_positive_values else plt.plot

    for values, style, label in series:
        plot(snr, values, style, label=label)


def plot_bler(data):
    snr = data["snr"]

    plt.figure()
    plot_metric(
        snr,
        [
            (data["bler_comb"], "o-", "С объединением (Chase)"),
            (data["bler_no"], "s--", "Без объединения"),
        ],
        semilogy=True,
    )

    plt.xlabel("SNR (дБ)")
    plt.ylabel("BLER (доля ошибок CRC)")
    plt.title("Зависимость BLER от SNR (CRC-24 фиксирован)")
    plt.grid(True, which="both", linestyle="--")
    plt.legend()


def plot_undetected(data):
    snr = data["snr"]

    plt.figure()
    plot_metric(
        snr,
        [
            (data["und_comb"], "o-", "С объединением (Chase)"),
            (data["und_no"], "s--", "Без объединения"),
        ],
        semilogy=True,
    )

    plt.xlabel("SNR (дБ)")
    plt.ylabel("Вероятность необнаруженной ошибки")
    plt.title("Необнаруженные ошибки CRC-24")
    plt.grid(True, which="both", linestyle="--")
    plt.legend()


def plot_goodput(data):
    snr = data["snr"]

    plt.figure()
    plot_metric(
        snr,
        [
            (data["gput_comb"], "o-", "С объединением (Chase)"),
            (data["gput_no"], "s--", "Без объединения"),
        ],
    )

    plt.xlabel("SNR (дБ)")
    plt.ylabel("Полезная пропускная способность (goodput)")
    plt.title("Пропускная способность системы")
    plt.grid(True, linestyle="--")
    plt.legend()


def parse_args():
    default_input = Path(__file__).with_name("cc.csv")
    parser = argparse.ArgumentParser(
        description="Plot Chase combining metrics and save figures."
    )
    parser.add_argument("--input", type=Path, default=default_input)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Directory for PNG files. Defaults to <csv-stem>_plots near the CSV.",
    )
    parser.add_argument("--show", action="store_true", help="Also show plots interactively.")
    return parser.parse_args()


def main():
    args = parse_args()
    output_dir = args.output_dir or args.input.with_suffix("").with_name(
        f"{args.input.stem}_plots"
    )

    data = load_data(args.input)

    saved = []
    plot_bler(data)
    saved.append(save_current_figure(output_dir, "chase_bler"))

    plot_undetected(data)
    saved.append(save_current_figure(output_dir, "chase_undetected"))

    plot_goodput(data)
    saved.append(save_current_figure(output_dir, "chase_goodput"))

    for path in saved:
        print(f"Saved {path}")

    if args.show:
        plt.show()
    else:
        plt.close("all")


if __name__ == "__main__":
    main()
