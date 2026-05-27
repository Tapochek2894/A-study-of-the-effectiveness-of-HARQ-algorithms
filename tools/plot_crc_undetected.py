#!/usr/bin/env python3
"""Plot CRC undetected-error probability vs SNR from crc_undetected_sim CSV.

CSV columns (long format): crc,r,mode,snr_db,p_undetected,p_detected,p_correct,
                           n_undetected,n_detected,n_correct,total_blocks

Прямой Монте-Карло не способен измерить P_undetected до 10⁻⁹ (для CRC-24A нужно
~10⁹ блоков). Но он точно измеряет ЧАСТОТУ ОШИБКИ КАДРА
  P_frame = (n_detected + n_undetected) / total,
а необнаруженная ошибка возникает, когда искажённый кадр случайно проходит CRC:
  P_undetected ≈ P_frame · 2⁻ʳ.
Поэтому по умолчанию строятся гладкие АНАЛИТИЧЕСКИЕ кривые P_frame·2⁻ʳ для всех
CRC (включая CRC-24A) до 10⁻⁹ и ниже, а поверх — РЕАЛЬНЫЕ Монте-Карло точки там,
где события наблюдались (валидация модели).
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


CRC_COLORS = {
    "3": "tab:purple",
    "8": "tab:red",
    "16": "tab:green",
    "24a": "tab:blue",
}
MODE_STYLE = {
    "uncoded": dict(linestyle="--", marker="v"),
    "coded": dict(linestyle="-", marker="o"),
}


def load_csv(path: Path):
    with path.open(newline="") as f:
        return [row for row in csv.DictReader(f) if row and "crc" in row]


def parse_args():
    parser = argparse.ArgumentParser(
        description="Plot CRC undetected probability vs SNR "
        "(analytic P_frame·2^-r + Monte-Carlo validation points)."
    )
    parser.add_argument("csv_path", help="CSV from crc_undetected_sim")
    parser.add_argument("--out", default=None, help="Output image path")
    parser.add_argument("--title", default=None, help="Plot title")
    parser.add_argument(
        "--modes", default="uncoded,coded",
        help="Comma list of modes to draw (default 'uncoded,coded').",
    )
    parser.add_argument(
        "--crc", default=None,
        help="Comma list of CRC presets to draw, e.g. '8,16,24a' "
             "(default: all present in CSV).",
    )
    parser.add_argument(
        "--floor", type=float, default=1e-9,
        help="Lower y-limit / floor for zero MC values (default 1e-9).",
    )
    parser.add_argument(
        "--measured-only", action="store_true",
        help="Draw only measured Monte-Carlo points, no analytic curves.",
    )
    parser.add_argument(
        "--no-analytic", action="store_true",
        help="Alias of --measured-only.",
    )
    parser.add_argument(
        "--no-ceilings", action="store_true",
        help="Do not draw 2^-r CRC undetected ceilings.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    csv_path = Path(args.csv_path)
    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    rows = load_csv(csv_path)
    if not rows:
        raise SystemExit("No data found in CSV file.")

    wanted_modes = [m for m in args.modes.split(",") if m]
    wanted_crcs = [c for c in args.crc.split(",") if c] if args.crc else None
    show_analytic = not (args.measured_only or args.no_analytic)

    # groups[(crc, mode)][snr] = {"p_und", "n_und", "p_frame"}
    groups = defaultdict(dict)
    crc_r = {}
    crc_order = []
    for row in rows:
        crc, mode = row["crc"], row["mode"]
        if mode not in wanted_modes:
            continue
        if wanted_crcs is not None and crc not in wanted_crcs:
            continue
        if crc not in crc_r:
            crc_r[crc] = int(row["r"])
            crc_order.append(crc)
        total = float(row["total_blocks"])
        n_und = int(row["n_undetected"])
        n_det = int(row["n_detected"])
        groups[(crc, mode)][float(row["snr_db"])] = {
            "p_und": float(row["p_undetected"]),
            "n_und": n_und,
            "p_frame": (n_und + n_det) / total if total else 0.0,
        }

    if not groups:
        raise SystemExit("No matching (crc, mode) series to plot.")

    fig, ax = plt.subplots(figsize=(10, 6))

    for crc in crc_order:
        color = CRC_COLORS.get(crc)
        r = crc_r[crc]
        for mode in wanted_modes:
            series = groups.get((crc, mode))
            if not series:
                continue
            snr = sorted(series)
            style = MODE_STYLE.get(mode, dict(linestyle="-", marker="o"))

            if show_analytic:
                # Гладкая теоретическая кривая P_frame·2^-r.
                y = [series[s]["p_frame"] * (2.0 ** -r) for s in snr]
                y = [v if v > 0 else args.floor for v in y]
                ax.semilogy(snr, y, color=color, linewidth=1.8,
                            linestyle="-",
                            label=f"CRC-{crc.upper()} (теория)")
                # Реальные MC-измерения (линия с маркерами) там, где были события.
                mx = [s for s in snr if series[s]["n_und"] > 0]
                my = [series[s]["p_und"] for s in mx]
                if mx:
                    ax.semilogy(mx, my, color=color, linestyle="--",
                                linewidth=1.0, marker=style["marker"],
                                markersize=6, markeredgecolor="black",
                                markeredgewidth=0.5, zorder=5,
                                label=f"CRC-{crc.upper()} (эксперимент)")
            else:
                # Только измерения: нули — на floor.
                y = [series[s]["p_und"] if series[s]["p_und"] > 0 else args.floor
                     for s in snr]
                ax.semilogy(snr, y, color=color, markersize=5,
                            label=f"CRC-{crc.upper()}, {mode}", **style)

    # Горизонтальные ориентиры 2^-r (потолок необнаружения каждого CRC).
    if not args.no_ceilings:
        for crc in crc_order:
            ceil = 2.0 ** (-crc_r[crc])
            if ceil >= args.floor:
                ax.axhline(ceil, color=CRC_COLORS.get(crc, "gray"),
                           linestyle=":", linewidth=0.9, alpha=0.5)
                ax.text(0.995, ceil, f" 2^-{crc_r[crc]}",
                        transform=ax.get_yaxis_transform(),
                        va="center", ha="left", fontsize=7,
                        color=CRC_COLORS.get(crc, "gray"))

    ax.set_xlabel("Отношение сигнал/шум (дБ)")
    ax.set_ylabel("Вероятность не обнаружить ошибку")
    default_title = "Моделирование для CRC различной длины в канале с замираниями"
    ax.set_title(args.title or default_title)
    ax.set_ylim(bottom=args.floor)
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend(loc="lower left", fontsize=8, ncol=2)

    out_path = Path(args.out) if args.out else csv_path.with_name(
        csv_path.stem + "_undetected.png"
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"Plot saved to: {out_path}")


if __name__ == "__main__":
    main()
