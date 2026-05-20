# Симулятор `crc_undetected_sim`

Считает вероятность **необнаруженной ошибки CRC** (`P_undetected`) и сопутствующие метрики (`P_detected`, `P_correct`) в зависимости от SNR. Канал — BPSK + когерентный Rayleigh. По умолчанию используется CRC-24A (см. `crc24_rationale.md`) и информационный блок `k = 512` бит. Два режима моделируются на одной сетке SNR:

- **uncoded** — без свёрточного кода: `message → CRC → BPSK → Rayleigh → hard decision → CRC check`.
- **coded** — со свёрточным кодом через AFF3CT: `message → CRC → conv (1/2, Viterbi) → BPSK → Rayleigh → LLR → conv soft-decode → CRC check`.

Классификация исхода блока:
- `correct` — CRC прошёл и декодированные информационные биты совпали с исходными;
- `detected` — CRC обнаружил ошибку (синдром ≠ 0);
- `undetected` — CRC прошёл, но информационная часть искажена (это и есть событие ошибки декодирования CRC, важное для HARQ).

## Сборка

Режим `coded` требует AFF3CT, иначе симулятор завершится с ошибкой ещё до старта.

```bash
cmake -S . -B build-aff3ct -DENABLE_AFF3CT=ON -DBUILD_TESTING=OFF
cmake --build build-aff3ct -j1 --target crc_undetected_sim
```

## Запуск

```bash
# Обе кривые (uncoded + coded) на одном CSV
./build-aff3ct/crc_undetected_sim \
    --blocks 50000 \
    --snr-start 0 --snr-end 20 --snr-points 11 \
    > metrics/crc/crc_undetected.csv

# Только uncoded (можно собирать без AFF3CT)
./build/crc_undetected_sim --mode uncoded --blocks 100000 \
    --snr 0,4,8,12,16,20 > metrics/crc/crc_undetected_unc.csv

# Только coded
./build-aff3ct/crc_undetected_sim --mode coded --blocks 50000 \
    --snr 0,4,8,12,16,20 > metrics/crc/crc_undetected_cod.csv
```

## Параметры

- `--blocks <count>` — число информационных блоков на каждую точку SNR (default `50000`).
- `--seed <seed>` — seed PRNG.
- `--info-bits <k>` — длина информационного сообщения в битах (default `512`).
- `--crc <24a|16>` — пресет CRC (default `24a`). `24a` соответствует 3GPP TS 38.212, `16` — CCITT/3GPP CRC-16.
- `--mode <uncoded|coded|both>` — какие кривые считать (default `both`).
- `--conv-rate <num/den>` — frame rate свёрточного кода (default `1/2`).
- `--conv-decoder <viterbi|bcjr>` — тип декодера AFF3CT (default `viterbi`).
- `--block-size <symbols>` — длина блока когерентности замираний (`1` — fast fading; `N` — flat fading).
- `--snr <dB1,dB2,...>` либо `--snr-start --snr-end --snr-points` — сетка SNR.

## Формат CSV

```
snr_db,
  [p_undetected_unc,p_detected_unc,p_correct_unc,
   undetected_unc,detected_unc,correct_unc,]
  [p_undetected_cod,p_detected_cod,p_correct_cod,
   undetected_cod,detected_cod,correct_cod,]
total_blocks
```

Колонки `*_unc` присутствуют только при `--mode uncoded|both`, `*_cod` — только при `--mode coded|both`.

## Построение графика

```bash
python3 tools/plot_crc_undetected.py metrics/crc/crc_undetected.csv \
    --out metrics/crc/crc_undetected.png
# Добавить P_detected (CRC fail rate):
python3 tools/plot_crc_undetected.py metrics/crc/crc_undetected.csv \
    --with-detected --out metrics/crc/crc_undetected_full.png
```

## Замечания

- При больших SNR в режиме `coded` событий `undetected` может вообще не быть на разумном числе блоков — это ожидаемо: вероятность необнаруженной ошибки для CRC-24A ограничена сверху `2⁻²⁴ ≈ 6·10⁻⁸`. Plot-скрипт по умолчанию подставляет `floor=1e-7` для нулевых отсчётов на лог-шкале (можно поменять через `--floor`).
- Параметры свёрточного кода AFF3CT берутся по умолчанию (`RSC`, `tail_length=6`); конкретный полином определяется самим AFF3CT.
