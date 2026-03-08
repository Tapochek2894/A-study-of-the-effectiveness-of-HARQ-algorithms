## Проект HARQ

### Сборка (CMake)
```
cmake -S . -B build
cmake --build build
```

### Тесты
```
ctest --test-dir build
```

### FEC (текущий статус)
- Введён frame-based интерфейс кодека (`IFecCodec`).
- Поддерживается backend `hamming`.
- Добавлен каркас backend `conv` (AFF3CT) с fallback при недоступной зависимости.

### Симулятор `bpsk_awgn_sim`
Режимы:
- uncoded: без `--r` и без `--codec conv`;
- coded hamming: `--codec hamming --r <parity_bits>`;
- coded conv (каркас): `--codec conv --conv-k <bits> --conv-rate <num/den> --conv-decoder <viterbi|bcjr>`.

Примеры:
```bash
# uncoded
./build/bpsk_awgn_sim --bits 200000 --snr 0,2,4,6

# coded hamming (7,4)
./build/bpsk_awgn_sim --bits 200000 --codec hamming --r 3 --snr 0,2,4,6

# coded conv (пока требует AFF3CT и завершится с понятной ошибкой без него)
./build/bpsk_awgn_sim --bits 200000 --codec conv --conv-k 1024 --conv-rate 1/2 --conv-decoder viterbi --snr 0,2,4,6
```

### Дополнительные симуляторы
- `qpsk_awgn_sim`: подробности в `docs/qpsk_awgn_sim.md`
- `qpsk_bpsk_sim`: подробности в `docs/qpsk_bpsk_sim.md`
- `ber_bler_sim`: подробности в `docs/ber_bler_sim.md`
