#!/usr/bin/env python3
"""Сравнение CRC-кодов и обоснование выбора CRC-24A для HARQ.

Считаем точный весовой спектр для k=8 (как в методичке Буркова-2022, §1.2)
и сравниваем характеристики обнаружения ошибок для:
  - CRC-3   (Бурков №1):  x³+x+1                  → r=3
  - CRC-8   (стандарт):   x⁸+x²+x+1               → r=8
  - CRC-16  (Бурков №6):  x¹⁶+x¹³+x¹²+x¹¹+x¹⁰
                          +x⁸+x⁶+x⁵+x²+1          → r=16
  - CRC-24A (3GPP 36.212/38.212):
            x²⁴+x²³+x¹⁸+x¹⁷+x¹⁴+x¹¹+x¹⁰
            +x⁷+x⁶+x⁵+x⁴+x³+x+1                   → r=24

Графики (в metrics/crc/):
  pud_vs_p.png         — P_ud(p), точный расчёт по весовому спектру.
  catastrophic_arq.png — вероятность доставки сообщения с необнаруженной
                          ошибкой при ARQ-Stop&Wait: P_ud/((1−p)ⁿ+P_ud).
  overhead_vs_pud.png  — компромисс «оверхед r/k» vs «асимптотическая
                          граница P_ud ≤ 2^−r».

Используем формулы из Буркова-2022 §1.2 / Буркова-2025 §1.1:
  P_ud(p)  = Σ_{i=d..n} A_i · pⁱ · (1−p)^(n−i)
  P_ud ≤ (1/2)^r              (асимптотика при p→1/2)
"""
from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
from typing import Dict, List, Tuple

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


# Полиномы как целые числа, MSB слева, бит на позиции r.
CRC_DEFS: Dict[str, int] = {
    # x³ + x + 1
    "CRC-3":   0b1011,
    # x⁸ + x² + x + 1  (8-бит стандарт)
    "CRC-8":   0b1_0000_0111,
    # x¹⁶+x¹³+x¹²+x¹¹+x¹⁰+x⁸+x⁶+x⁵+x²+1  (Бурков 2022, табл. 2.1, №6)
    "CRC-16":  0x13D65,
    # 3GPP TS 36.212 / 38.212, CRC24A:
    # x²⁴+x²³+x¹⁸+x¹⁷+x¹⁴+x¹¹+x¹⁰+x⁷+x⁶+x⁵+x⁴+x³+x+1
    "CRC-24A": 0x1864CFB,
}

CRC_STYLES = {
    "CRC-3":   {"color": "#1f77b4", "marker": "o", "linestyle": "-"},
    "CRC-8":   {"color": "#2ca02c", "marker": "s", "linestyle": "-"},
    "CRC-16":  {"color": "#ff7f0e", "marker": "^", "linestyle": "-"},
    "CRC-24A": {"color": "#d62728", "marker": "D", "linestyle": "-"},
}


def crc_degree(poly: int) -> int:
    return poly.bit_length() - 1


def encode(message: int, k: int, poly: int) -> int:
    """Кодирование систематическим CRC: a(x) = m(x)·xʳ + (m(x)·xʳ mod g(x)).

    На вход целое m длиной k бит, на выход — целое кодового слова длиной n=k+r.
    """
    r = crc_degree(poly)
    shifted = message << r
    rem = shifted
    # деление в столбик; xor при выставленном старшем бите.
    for i in range(k - 1, -1, -1):
        bit_pos = i + r
        if (rem >> bit_pos) & 1:
            rem ^= poly << i
    return shifted | rem  # m·xʳ + остаток


def weight_spectrum(k: int, poly: int) -> Dict[int, int]:
    """Точный весовой спектр {вес: число кодовых слов} перебором всех 2^k.

    Возвращает Counter — A_0 = 1, A_i для i ≥ 1.
    """
    spectrum: Counter = Counter()
    for m in range(1 << k):
        cw = encode(m, k, poly)
        spectrum[bin(cw).count("1")] += 1
    return dict(spectrum)


def pud_from_spectrum(spectrum: Dict[int, int], n: int,
                      p: np.ndarray) -> np.ndarray:
    """P_ud(p) = Σ_{i≥1} A_i · pⁱ · (1−p)^(n−i)."""
    p = np.asarray(p, dtype=float)
    pud = np.zeros_like(p)
    for w, count in spectrum.items():
        if w == 0 or count == 0:
            continue
        pud += count * (p ** w) * ((1.0 - p) ** (n - w))
    return pud


def d_min_from_spectrum(spectrum: Dict[int, int]) -> int:
    for w in sorted(spectrum.keys()):
        if w > 0 and spectrum[w] > 0:
            return w
    return -1


def compute_for_all(k: int) -> Dict[str, dict]:
    """Считает n, d_min, спектр для всех CRC при заданной длине информации k."""
    results: Dict[str, dict] = {}
    for name, poly in CRC_DEFS.items():
        r = crc_degree(poly)
        spec = weight_spectrum(k, poly)
        results[name] = {
            "poly": poly,
            "r": r,
            "n": k + r,
            "d_min": d_min_from_spectrum(spec),
            "spectrum": spec,
        }
    return results


def plot_pud_vs_p(results: dict, k: int, out_path: Path) -> None:
    p = np.logspace(-6, np.log10(0.5), 240)
    fig, ax = plt.subplots(figsize=(8, 5.5))
    for name, info in results.items():
        pud = pud_from_spectrum(info["spectrum"], info["n"], p)
        st = CRC_STYLES[name]
        ax.plot(p, pud, color=st["color"], linestyle=st["linestyle"],
                linewidth=1.6,
                label=(f"{name}  (r={info['r']}, "
                       f"n={info['n']}, d_min={info['d_min']})"))
        # асимптотическая граница 2^-r
        ax.axhline(0.5 ** info["r"], color=st["color"],
                   linestyle=":", linewidth=1.0, alpha=0.6)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Вероятность ошибки в канале p")
    ax.set_ylabel("P_ud — вероятность необнаруженной ошибки")
    ax.set_title(f"P_ud(p) для разных CRC при k={k} (пунктир — 2⁻ʳ)")
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.5)
    ax.legend(loc="lower right", fontsize=9)
    ax.set_ylim(1e-12, 1)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(out_path)


def plot_catastrophic_arq(results: dict, k: int, out_path: Path) -> None:
    """Вероятность принять заведомо ошибочное сообщение в Stop&Wait ARQ.

    P_catastrophic = P_ud / ((1-p)^n + P_ud).
    """
    p = np.logspace(-6, -1, 240)
    fig, ax = plt.subplots(figsize=(8, 5.5))
    for name, info in results.items():
        n = info["n"]
        pud = pud_from_spectrum(info["spectrum"], n, p)
        success = (1.0 - p) ** n
        cat = pud / (success + pud)
        st = CRC_STYLES[name]
        ax.plot(p, cat, color=st["color"], linestyle=st["linestyle"],
                linewidth=1.6, label=f"{name}  (n={n})")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Вероятность ошибки в канале p")
    ax.set_ylabel("P(катастрофа в ARQ) = P_ud / ((1−p)ⁿ + P_ud)")
    ax.set_title(f"Доставленные с ошибкой пакеты в ARQ Stop&Wait, k={k}")
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.5)
    ax.legend()
    ax.set_ylim(1e-12, 1)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(out_path)


def plot_overhead_vs_pud(out_path: Path,
                         k_values: Tuple[int, ...] = (24, 100, 1000, 8000)
                         ) -> None:
    """График «оверхед r/k vs защищённость P_ud ≤ 2^-r» — для разных k.

    Показывает, что CRC-24 имеет пренебрежимый оверхед при k ≥ ~100,
    но даёт асимптотику P_ud ≤ 6·10⁻⁸.
    """
    fig, ax = plt.subplots(figsize=(8, 5.5))
    r_values = np.array(sorted({crc_degree(p) for p in CRC_DEFS.values()}))
    pud_bound = 0.5 ** r_values

    for k in k_values:
        overhead = r_values / k * 100.0  # в %
        ax.plot(overhead, pud_bound, marker="o", linewidth=1.5,
                label=f"k = {k} бит")
        for r, oh, pud in zip(r_values, overhead, pud_bound):
            ax.annotate(f"r={r}", (oh, pud), textcoords="offset points",
                        xytext=(4, 4), fontsize=8, alpha=0.7)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Оверхед r/k, %")
    ax.set_ylabel("Асимптотическая граница P_ud = 2⁻ʳ")
    ax.set_title("Компромисс «оверхед vs защищённость» для разных длин блока")
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.5)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(out_path)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--k", type=int, default=8,
                   help="Длина информационной части в битах для точного спектра "
                        "(по умолчанию 8 — как в методичке Буркова).")
    p.add_argument("--out-dir", type=str, default="metrics/crc",
                   help="Каталог для сохранения графиков.")
    return p.parse_args()


def main() -> None:
    args = parse_args()
    if args.k < 2 or args.k > 22:
        raise SystemExit("k должно быть в диапазоне [2, 22] для разумного перебора.")
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"[1/3] Считаю весовые спектры всех CRC при k={args.k}...")
    results = compute_for_all(args.k)
    for name, info in results.items():
        spec_short = {w: c for w, c in info["spectrum"].items()
                      if w in (0, info["d_min"]) or c > 0}
        first_terms = sorted((w, info["spectrum"][w]) for w in info["spectrum"]
                             if w <= info["d_min"] + 2)
        print(f"  {name}: r={info['r']}, n={info['n']}, d_min={info['d_min']}, "
              f"первые члены спектра {first_terms}")

    print("[2/3] Графики P_ud и катастрофы ARQ...")
    plot_pud_vs_p(results, args.k, out_dir / "pud_vs_p.png")
    plot_catastrophic_arq(results, args.k, out_dir / "catastrophic_arq.png")
    print("[3/3] График «оверхед vs защищённость»...")
    plot_overhead_vs_pud(out_dir / "overhead_vs_pud.png")
    print("Готово.")


if __name__ == "__main__":
    main()
