#!/usr/bin/env python3
"""Теоретический расчёт для CRC в BSC-канале (#37).

Вычисляет:
  1. Весовой спектр кода A_i — число кодовых слов веса i
  2. P_ud(p)  — вероятность необнаруженной ошибки
  3. P_d(p)   — вероятность обнаруженной ошибки
  4. E[N](p)  — среднее число передач ARQ
  5. P(N=k,p) — ряд вероятностей числа передач
  6. Сравнение с симуляцией (если передан CSV)

Использование:
  python3 tools/crc_theory.py \\
      --poly 1,0,1,1 --msg-len 8 --max-retx 10 \\
      --sim /tmp/sim_crc3.csv \\
      --out-prefix metrics/CRC3_theory
"""

import argparse
import csv
import itertools
import math
from pathlib import Path
from scipy.special import erfc

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


# ---------------------------------------------------------------------------
# GF(2) арифметика
# ---------------------------------------------------------------------------

def snr_db_to_ber(snr_db: float) -> float:
    """BER для BPSK в AWGN: p = Q(sqrt(2*Eb/N0)) = 0.5*erfc(sqrt(Eb/N0))."""
    eb_n0 = 10 ** (snr_db / 10.0)
    return 0.5 * erfc(math.sqrt(eb_n0))


def poly_mod_gf2(dividend: list[int], divisor: list[int]) -> list[int]:
    """Остаток от деления dividend на divisor в GF(2)."""
    rem = list(dividend)
    dlen = len(divisor)
    while len(rem) >= dlen:
        if rem[0] == 1:
            for i in range(dlen):
                rem[i] ^= divisor[i]
        rem.pop(0)
    return rem


def crc_encode(message: list[int], poly: list[int]) -> list[int]:
    """Кодирование CRC: возвращает кодовое слово [message | checkbits]."""
    r = len(poly) - 1
    padded = list(message) + [0] * r
    remainder = poly_mod_gf2(padded, poly)
    # дополняем нулями слева до длины r
    remainder = [0] * (r - len(remainder)) + remainder
    return list(message) + remainder


def hamming_weight(bits: list[int]) -> int:
    return sum(bits)


# ---------------------------------------------------------------------------
# Весовой спектр
# ---------------------------------------------------------------------------

def compute_weight_spectrum(poly: list[int], k: int) -> dict[int, int]:
    """A_i = число кодовых слов веса i (включая нулевое).

    Перебирает все 2^k сообщений, кодирует и считает вес.
    Работает для небольших k (≤ 20).
    """
    spectrum: dict[int, int] = {}
    n = k + len(poly) - 1
    for idx in range(1 << k):
        msg = [(idx >> i) & 1 for i in range(k)]
        cw = crc_encode(msg, poly)
        w = hamming_weight(cw)
        spectrum[w] = spectrum.get(w, 0) + 1
    return spectrum


# ---------------------------------------------------------------------------
# Теоретические вероятности
# ---------------------------------------------------------------------------

def p_undetected(spectrum: dict[int, int], n: int, p: float) -> float:
    """P_ud = sum_{i=1}^{n} A_i * p^i * (1-p)^(n-i)

    Сумма по ненулевым кодовым словам (индикатор того, что
    паттерн ошибок совпал с ненулевым кодовым словом).
    """
    q = 1.0 - p
    result = 0.0
    for w, count in spectrum.items():
        if w == 0:
            continue
        result += count * (p ** w) * (q ** (n - w))
    return result


def p_block_error(n: int, p: float) -> float:
    """BLER = 1 - (1-p)^n — хотя бы одна ошибка в блоке."""
    return 1.0 - (1.0 - p) ** n


def p_detected(spectrum: dict[int, int], n: int, p: float) -> float:
    return p_block_error(n, p) - p_undetected(spectrum, n, p)


def avg_retx(p_d: float, max_retx: int) -> float:
    """E[N] = [1 - p_d^(M+1)] / (1 - p_d), M = max_retx."""
    if p_d >= 1.0:
        return max_retx + 1
    if p_d <= 0.0:
        return 1.0
    M = max_retx
    return (1.0 - p_d ** (M + 1)) / (1.0 - p_d)


def retx_distribution(p_d: float, max_retx: int) -> list[float]:
    """P(N=k) для k = 1..max_retx+1."""
    probs = []
    for k in range(1, max_retx + 1):
        probs.append((p_d ** (k - 1)) * (1.0 - p_d))
    probs.append(p_d ** max_retx)   # k = max_retx + 1 (исчерпали)
    return probs


# ---------------------------------------------------------------------------
# Загрузка CSV симуляции
# ---------------------------------------------------------------------------

def load_sim_csv(path: Path) -> dict[str, list[float]]:
    data: dict[str, list[float]] = {}
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            for k, v in row.items():
                if v.strip() == "":
                    continue
                data.setdefault(k, []).append(float(v))
    return data


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_poly(s: str) -> list[int]:
    return [int(x) for x in s.split(",")]


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--poly", required=True, help="CRC polynomial, e.g. 1,0,1,1")
    p.add_argument("--msg-len", type=int, default=8, help="Info bits k (default: 8)")
    p.add_argument("--max-retx", type=int, default=10, help="Max retransmissions")
    p.add_argument("--sim", help="CSV from crc_ber_bler_sim for comparison")
    p.add_argument("--out-prefix", default="metrics/crc_theory",
                   help="Output file prefix (default: metrics/crc_theory)")
    p.add_argument("--title", default="", help="Extra title prefix")
    return p.parse_args()


def main() -> None:
    args = parse_args()
    poly = parse_poly(args.poly)
    k = args.msg_len
    r = len(poly) - 1
    n = k + r
    M = args.max_retx
    prefix = args.out_prefix
    label = args.title or f"CRC-{r} (x^{r}+...+1), k={k}, n={n}"

    print(f"Polynomial: {poly}  r={r}  k={k}  n={n}")

    # 1. Весовой спектр
    print("Computing weight spectrum...")
    spectrum = compute_weight_spectrum(poly, k)
    total_cw = sum(spectrum.values())
    print(f"  Total codewords (incl. zero): {total_cw}  (expected 2^{k}={1<<k})")
    print(f"  Weight spectrum A_i:")
    for w in sorted(spectrum):
        print(f"    A_{w:2d} = {spectrum[w]}")
    print(f"  Minimum distance d_min = {min(w for w in spectrum if w > 0)}")

    # 2. Теоретические кривые по p
    p_values = np.logspace(-4, np.log10(0.4), 200)

    pud_theory = np.array([p_undetected(spectrum, n, p) for p in p_values])
    bler_theory = np.array([p_block_error(n, p) for p in p_values])
    pd_theory   = np.array([p_detected(spectrum, n, p) for p in p_values])
    en_theory   = np.array([avg_retx(float(pd), M) for pd in pd_theory])

    # 3. Симуляция (если есть)
    sim = None
    if args.sim:
        sim = load_sim_csv(Path(args.sim))

    # ---------------------------------------------------------------------------
    # График 1: весовой спектр (столбчатый)
    # ---------------------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(9, 5))
    weights = sorted(w for w in spectrum if w > 0)
    counts  = [spectrum[w] for w in weights]
    ax.bar(weights, counts, color="steelblue", edgecolor="black", width=0.6)
    ax.set_xlabel("Вес кодового слова i")
    ax.set_ylabel("Число кодовых слов $A_i$")
    ax.set_title(f"Весовой спектр: {label}")
    ax.set_xticks(weights)
    ax.grid(axis="y", linestyle="--", alpha=0.5)
    for w, c in zip(weights, counts):
        ax.text(w, c + 0.3, str(c), ha="center", va="bottom", fontsize=9)
    fig.tight_layout()
    out1 = f"{prefix}_spectrum.png"
    fig.savefig(out1, dpi=150)
    plt.close(fig)
    print(f"Saved: {out1}")

    # ---------------------------------------------------------------------------
    # График 2: P_ud, P_d, BLER vs p (теория + симуляция)
    # ---------------------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.loglog(p_values, bler_theory,  "--",  color="gray",    label="BLER (теория)")
    ax.loglog(p_values, pd_theory,    "-",   color="steelblue", label="$P_d$ обнаружено (теория)")
    ax.loglog(p_values, pud_theory,   "-",   color="crimson",   label="$P_{ud}$ необнаружено (теория)")

    if sim:
        sp = np.array(sim.get("p", []))
        if "p_detected" in sim:
            ax.scatter(sp, sim["p_detected"],   marker="o", s=15, color="steelblue",
                       label="$P_d$ (симуляция)", zorder=5)
        if "p_undetected" in sim:
            ax.scatter(sp, sim["p_undetected"], marker="s", s=15, color="crimson",
                       label="$P_{ud}$ (симуляция)", zorder=5)
        if "bler" in sim:
            ax.scatter(sp, sim["bler"],         marker="^", s=15, color="gray",
                       label="BLER (симуляция)", zorder=5)

    ax.set_xlabel("Вероятность ошибки бита p")
    ax.set_ylabel("Вероятность")
    ax.set_title(f"Вероятности ошибок: {label}")
    ax.legend(fontsize=8)
    ax.grid(True, which="both", linestyle="--", alpha=0.4)
    fig.tight_layout()
    out2 = f"{prefix}_probs.png"
    fig.savefig(out2, dpi=150)
    plt.close(fig)
    print(f"Saved: {out2}")

    # ---------------------------------------------------------------------------
    # График 3: E[N] — среднее число передач
    # ---------------------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.semilogx(p_values, en_theory, "-", color="steelblue",
                label=f"E[N] теория (max_retx={M})")
    ax.axhline(1.0, linestyle=":", color="gray", alpha=0.6)
    ax.axhline(M + 1, linestyle=":", color="gray", alpha=0.6)
    ax.set_xlabel("Вероятность ошибки бита p")
    ax.set_ylabel("Среднее число передач E[N]")
    ax.set_title(f"Среднее число передач ARQ: {label}")
    ax.legend(fontsize=9)
    ax.grid(True, which="both", linestyle="--", alpha=0.4)
    fig.tight_layout()
    out3 = f"{prefix}_avg_retx.png"
    fig.savefig(out3, dpi=150)
    plt.close(fig)
    print(f"Saved: {out3}")

    # ---------------------------------------------------------------------------
    # График 4: распределение P(N=k) при нескольких значениях SNR
    # ---------------------------------------------------------------------------
    snr_samples = [-2, 2, 4, 6]
    fig, axes = plt.subplots(1, len(snr_samples), figsize=(4 * len(snr_samples), 5))
    for ax, snr_db in zip(axes, snr_samples):
        p_val = snr_db_to_ber(snr_db)
        pd_val = p_detected(spectrum, n, p_val)
        probs = retx_distribution(pd_val, M)
        ks = list(range(1, M + 2))
        ax.bar(ks, probs, color="steelblue", edgecolor="black", width=0.6)
        ax.set_title(f"SNR = {snr_db} дБ\n$P_d$={pd_val:.3f}")
        ax.set_xlabel("Число передач N")
        ax.set_ylabel("P(N=k)")
        ax.set_xticks(ks[::max(1, M // 5)])
        ax.grid(axis="y", linestyle="--", alpha=0.5)
    fig.suptitle(f"Распределение числа передач ARQ: {label}")
    fig.tight_layout()
    out4 = f"{prefix}_retx_dist.png"
    fig.savefig(out4, dpi=150)
    plt.close(fig)
    print(f"Saved: {out4}")


if __name__ == "__main__":
    main()
