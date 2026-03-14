# Сравнение QPSK и BPSK

Утилита `qpsk_bpsk_sim` сравнивает BER для BPSK и QPSK на одной сетке SNR.
Поддерживает uncoded и coded режим через общий FEC-интерфейс.

## Сборка

```bash
cmake -S . -B build
cmake --build build --target qpsk_bpsk_sim
```

## Запуск

```bash
# оба типа модуляции, uncoded
./build/qpsk_bpsk_sim --bits 200000 --snr 0,2,4,6 --both

# только BPSK, coded hamming
./build/qpsk_bpsk_sim --bits 200000 --codec hamming --r 3 --snr 0,2,4,6 --bpsk

# только QPSK, coded conv
./build/qpsk_bpsk_sim --bits 200000 --codec conv --conv-k 1024 --conv-rate 1/2 --conv-decoder viterbi --snr 0,2,4,6 --qpsk
```

## Параметры

- `--bits <count>`: число информационных бит.
- `--seed <seed>`: seed генератора.
- `--snr <dB1,dB2,...>`: список SNR в dB.
- `--snr-start <dB> --snr-end <dB> --snr-points <n>`: диапазон SNR.
- `--r <parity_bits>`: параметр Хэмминга для `--codec hamming`.
- `--codec <hamming|conv>`: backend кодека.
- `--conv-k <bits>`: число входных бит кадра для `conv`.
- `--conv-rate <num/den>`: frame rate для `conv`.
- `--conv-decoder <viterbi|bcjr>`: тип декодера для `conv`.
- `--bpsk | --qpsk | --both`: выбор модуляции.

## Выход CSV

- uncoded: `snr_db,bpsk_ber,bpsk_errors,qpsk_ber,qpsk_errors,total_bits`
- coded: `snr_db,bpsk_ber_uncoded,bpsk_ber_coded,bpsk_errors_uncoded,bpsk_errors_coded,qpsk_ber_uncoded,qpsk_ber_coded,qpsk_errors_uncoded,qpsk_errors_coded,total_bits`

Фактические столбцы зависят от выбранных флагов `--bpsk/--qpsk/--both`.

## Ограничения

- Режим `--codec conv` требует сборки с `ENABLE_AFF3CT=ON` (AFF3CT скачивается CMake автоматически).
