import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


SERIES = [
    ("no", "ARQ RV0 1/2 без объединения", "^--"),
    ("chase_full", "Chase full codeword", "o-"),
    ("chase_punctured", "Chase RV0 1/2", "D-"),
    ("ir", "Incremental Redundancy", "s-"),
]


LEGACY_SERIES = {
    "chase": ("Chase Combining", "o-"),
    "cc": ("Chase Combining", "o-"),
}


def load_data(filename):
    data = {}

    with open(filename, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            for name, value in row.items():
                data.setdefault(name, []).append(float(value))

    return data


def column(data, *names):
    for name in names:
        if name in data:
            return data[name]
    raise KeyError(f"CSV does not contain any of columns: {', '.join(names)}")


def save_current_figure(output_dir, name):
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / f"{name}.png"
    plt.tight_layout()
    plt.savefig(path, dpi=180)
    return path


def clipped(values, drop_tail):
    if drop_tail <= 0:
        return values
    return values[:-drop_tail]


def plot_series(data, metric, ylabel, title, semilogy=False, drop_tail=0):
    snr = clipped(column(data, "snr", "snr_db"), drop_tail)

    plt.figure()
    series = []

    for suffix, label, style in SERIES:
        name = f"{metric}_{suffix}"
        if name in data:
            series.append((clipped(data[name], drop_tail), style, label))

    for suffix, (label, style) in LEGACY_SERIES.items():
        name = f"{metric}_{suffix}"
        if name in data:
            series.append((clipped(data[name], drop_tail), style, label))

    has_positive_values = any(any(value > 0.0 for value in values) for values, _, _ in series)
    plot = plt.semilogy if semilogy and has_positive_values else plt.plot

    for values, style, label in series:
        plot(snr, values, style, label=label)

    plt.xlabel("SNR (дБ)")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True, which="both" if semilogy else "major", linestyle="--")
    plt.legend()


def plot_bler(data, drop_tail):
    plot_series(
        data,
        "bler",
        "BLER (доля ошибок CRC)",
        "BLER vs SNR",
        semilogy=True,
        drop_tail=drop_tail,
    )


def plot_undetected(data, drop_tail):
    plot_series(
        data,
        "und",
        "Вероятность необнаруженной ошибки",
        "CRC-24: необнаруженные ошибки",
        semilogy=True,
        drop_tail=drop_tail,
    )


def plot_goodput(data, drop_tail):
    plot_series(
        data,
        "gput",
        "Полезная пропускная способность (goodput)",
        "Goodput vs SNR",
        drop_tail=drop_tail,
    )


def parse_args():
    default_input = Path(__file__).with_name("sisisiHUGE.csv")
    parser = argparse.ArgumentParser(
        description="Plot HARQ scheme metrics and save figures."
    )
    parser.add_argument("--input", type=Path, default=default_input)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Directory for PNG files. Defaults to <csv-stem>_plots near the CSV.",
    )
    parser.add_argument(
        "--drop-tail",
        type=int,
        default=0,
        help="Ignore this many rows from the end of every series.",
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
    plot_bler(data, args.drop_tail)
    saved.append(save_current_figure(output_dir, "harq_bler"))

    plot_undetected(data, args.drop_tail)
    saved.append(save_current_figure(output_dir, "harq_undetected"))

    plot_goodput(data, args.drop_tail)
    saved.append(save_current_figure(output_dir, "harq_goodput"))

    for path in saved:
        print(f"Saved {path}")

    if args.show:
        plt.show()
    else:
        plt.close("all")


if __name__ == "__main__":
    main()
