#!/usr/bin/env python3
import argparse
import csv
import sys

import matplotlib.pyplot as plt


def read_points(csv_path, max_points):
    points = []
    bit0 = []
    bit1 = []
    with open(csv_path, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            bit = row.get("bit")
            i_val = row.get("i")
            q_val = row.get("q")
            if bit is None or i_val is None or q_val is None:
                continue
            try:
                bit_int = int(bit)
                i_float = float(i_val)
                q_float = float(q_val)
            except ValueError:
                continue
            points.append((i_float, q_float))
            if bit_int == 0:
                bit0.append((i_float, q_float))
            elif bit_int == 1:
                bit1.append((i_float, q_float))

    if not points:
        return points, bit0, bit1

    if max_points is None or len(points) <= max_points:
        return points, bit0, bit1

    stride = max(1, len(points) // max_points)
    points = points[::stride]
    bit0 = bit0[::stride]
    bit1 = bit1[::stride]
    return points, bit0, bit1


def mean_point(points):
    if not points:
        return None
    sum_i = sum(p[0] for p in points)
    sum_q = sum(p[1] for p in points)
    count = len(points)
    return sum_i / count, sum_q / count


def main():
    parser = argparse.ArgumentParser(
        description="Plot passband IQ scatter cloud from CSV."
    )
    parser.add_argument("input_csv", help="CSV with columns bit,i,q")
    parser.add_argument("output_png", help="Output PNG path")
    parser.add_argument("--limit", type=float, default=2.0, help="Axis limit")
    parser.add_argument(
        "--title",
        default="Облака рассеивания",
        help="Plot title",
    )
    parser.add_argument("--point-size", type=float, default=10.0, help="Point size")
    parser.add_argument("--alpha", type=float, default=0.35, help="Point alpha")
    parser.add_argument(
        "--max-points",
        type=int,
        default=50000,
        help="Max points to draw (downsampled by stride)",
    )
    args = parser.parse_args()

    points, bit0, bit1 = read_points(args.input_csv, args.max_points)
    if not points:
        print("No data found in CSV.", file=sys.stderr)
        return 1

    fig, ax = plt.subplots(figsize=(6.4, 4.8), facecolor="white")
    ax.set_facecolor("white")

    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    ax.scatter(
        xs,
        ys,
        s=args.point_size,
        c="#1f77b4",
        alpha=args.alpha,
        edgecolors="none",
        rasterized=True,
    )

    center_points = []
    bit0_center = mean_point(bit0)
    bit1_center = mean_point(bit1)
    if bit0_center:
        center_points.append(bit0_center)
    if bit1_center:
        center_points.append(bit1_center)
    if center_points:
        ax.scatter(
            [p[0] for p in center_points],
            [p[1] for p in center_points],
            s=90,
            c="#ff7f0e",
            marker="o",
            edgecolors="none",
            zorder=3,
        )

    ax.set_title(args.title, color="black", pad=8, fontsize=12)
    ax.set_xlabel("Синфазная составляющая (I)")
    ax.set_ylabel("Квадратурная составляющая (Q)")
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.7)
    for spine in ax.spines.values():
        spine.set_color("#000000")

    ax.set_aspect("equal", adjustable="box")
    ax.set_xlim(-args.limit, args.limit)
    ax.set_ylim(-args.limit, args.limit)

    plt.tight_layout()
    plt.savefig(args.output_png, dpi=160, facecolor=fig.get_facecolor())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
