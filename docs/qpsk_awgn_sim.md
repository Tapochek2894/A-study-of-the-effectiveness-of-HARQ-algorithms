# QPSK AWGN симулятор

Утилита `qpsk_awgn_sim` моделирует QPSK в канале AWGN и считает BER по SNR.
Поддерживает uncoded и coded режим через общий FEC-интерфейс.

## Сборка

```bash
cmake -S . -B build
cmake --build build --target qpsk_awgn_sim
```

## Запуск

```bash
# uncoded
./build/qpsk_awgn_sim --bits 200000 --snr 0,2,4,6

# coded hamming
./build/qpsk_awgn_sim --bits 200000 --codec hamming --r 3 --snr 0,2,4,6

# coded conv
./build/qpsk_awgn_sim --bits 200000 --codec conv --conv-k 1024 --conv-rate 1/2 --conv-decoder viterbi --snr 0,2,4,6
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

## Выход CSV

- uncoded: `snr_db,ber,bit_errors,total_bits`
- coded: `snr_db,ber_uncoded,ber_coded,bit_errors_uncoded,bit_errors_coded,total_bits_uncoded,total_bits_coded`

## Ограничения

- `--conv-decoder bcjr` сейчас использует fallback на Viterbi в текущем встроенном backend.
