# Симулятор `crc_undetected_sim`

Строит зависимость **вероятности необнаруженной ошибки CRC** (`P_undetected`) от
SNR для нескольких CRC одновременно. Канал — BPSK + когерентный Rayleigh.
Информационный блок по умолчанию `k = 512` бит. Каждый CRC моделируется в двух
режимах на общей сетке SNR:

- **uncoded** — без свёрточного кода: `message → CRC → BPSK → Rayleigh → hard decision → CRC check`.
- **coded** — со свёрточным кодом: `message → CRC → conv 1/3 → BPSK → Rayleigh → LLR → Viterbi (soft) → CRC check`.

Свёрточный код — **нативный несистематический** (rate 1/n) с декодером Витерби,
реализован в `harq::fec::ConvolutionalCcCodec`. По умолчанию это классический
код Оденвальдера **(133,171,165)₈, K = 7, rate 1/3, d_free = 15**. Кодек не
зависит от AFF3CT, поэтому симулятор полностью работает в базовой сборке.

Классификация исхода блока:
- `correct` — CRC прошёл и декодированные информационные биты совпали с исходными;
- `detected` — CRC обнаружил ошибку (синдром ≠ 0);
- `undetected` — CRC прошёл, но информационная часть искажена. **Это и есть
  событие «не обнаружить ошибку», основная метрика графика.**

## Физика и точность

`P_undetected ≈ P(frame error) · 2⁻ʳ`, где `r` — число проверочных бит CRC, а
`P(frame error)` — вероятность ошибки кадра после демодуляции/декодирования.
Отсюда практические следствия:

- Для **uncoded** при низком SNR почти все кадры ошибочны (`P(frame error) ≈ 1`),
  поэтому `P_undetected` выходит на «полку» `≈ 2⁻ʳ` — это потолок необнаружения
  данного CRC. На графике он дополнительно отмечен пунктиром.
- Для **coded** ошибок кадра меньше, и `P_undetected` падает с ростом SNR.
- Прямой Монте-Карло «видит» событие, только если успел его сгенерировать.
  Чтобы зафиксировать `P_undetected ≈ p`, нужно порядка `10/p` блоков на точку.
  Поэтому короткие CRC (CRC-8, потолок `2⁻⁸ ≈ 3.9·10⁻³`) дают богатую кривую уже
  на ~10⁵ блоков, CRC-16 (`2⁻¹⁶ ≈ 1.5·10⁻⁵`) требует ~10⁶–10⁷, а **CRC-24A**
  (`2⁻²⁴ ≈ 6·10⁻⁸`) на реалистичных прогонах останется на полу графика — это и
  есть демонстрация его силы (необнаруженная ошибка практически исключена).

## Сборка

AFF3CT не требуется — нативный CC компилируется всегда:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j6 --target crc_undetected_sim
```

Симулятор сам подхватит OpenMP, если он установлен (`find_package(OpenMP)` в
CMake), и распараллелит задания (CRC × SNR) по потокам.

## Запуск

```bash
# Сравнение трёх CRC, обе кривые (uncoded + coded) на одном CSV
./build/crc_undetected_sim \
    --blocks 500000 --crc 8,16,24a \
    --snr 0,3,6,9,12,15,18 \
    > metrics/crc/crc_undetected.csv

# Только coded, один CRC
./build/crc_undetected_sim --mode coded --crc 16 --blocks 1000000 \
    --snr-start 0 --snr-end 18 --snr-points 7 > out.csv
```

## Параметры

- `--blocks <count>` — число блоков на каждую точку (CRC × SNR), default `200000`.
- `--seed <seed>` — seed PRNG.
- `--info-bits <k>` — длина информационного сообщения в битах (default `512`).
- `--crc <list>` — список CRC через запятую из `8`, `16`, `24a` (default `8,16,24a`).
  `8` — CRC-8/CCITT (`x⁸+x²+x+1`), `16` — CCITT/3GPP CRC-16 (`x¹⁶+x¹²+x⁵+1`),
  `24a` — 3GPP TS 38.212 CRC-24A.
- `--mode <uncoded|coded|both>` — какие режимы считать (default `both`).
- `--cc-gens <octal list>` — генераторы свёрточного кода в восьмеричном виде
  (default `133,171,165` ⇒ rate 1/3). Число генераторов = знаменатель скорости.
- `--cc-K <K>` — длина кодового ограничения (default `7`).
- `--block-size <symbols|auto>` — длина блока когерентности замираний. `auto`
  (default) = flat block fading: блок равен длине фрейма каждого режима. `1` —
  fast fading, `>1` — фиксированный размер.
- `--snr <dB1,dB2,...>` либо `--snr-start --snr-end --snr-points` — сетка SNR.

## Формат CSV (long format)

Одна строка на каждое сочетание `(crc, mode, snr)`:

```
crc,r,mode,snr_db,p_undetected,p_detected,p_correct,n_undetected,n_detected,n_correct,total_blocks
```

В параллельном режиме порядок строк нестрог — каждое задание стримится в stdout
сразу после завершения (при Ctrl+C уже посчитанное не теряется). Plot-скрипт
группирует строки по `(crc, mode)`, порядок не важен.

## Построение графика

```bash
# P_undetected (по умолчанию), все CRC и оба режима, ось до 10⁻⁹
python3 tools/plot_crc_undetected.py metrics/crc/crc_undetected.csv \
    --out metrics/crc/crc_undetected.png

# Только coded
python3 tools/plot_crc_undetected.py metrics/crc/crc_undetected.csv \
    --modes coded --out metrics/crc/crc_undetected_coded.png

# Другая метрика
python3 tools/plot_crc_undetected.py metrics/crc/crc_undetected.csv \
    --metric detected --out metrics/crc/crc_detected.png
```

По умолчанию: цвет = CRC, стиль линии = режим (uncoded — пунктир, coded —
сплошная), нули заменяются на `--floor` (default `1e-9`) и так же задаётся нижняя
граница оси Y. Для `P_undetected` рисуются пунктирные ориентиры `2⁻ʳ` каждого CRC.

## Замечания

- Метрика честная: чем сильнее CRC (больше `r`), тем ниже `P_undetected`, вплоть
  до того, что событие не наблюдается за разумное число блоков (CRC-24A).
- Полка uncoded-кривой на уровне `2⁻ʳ` — наглядное подтверждение верхней границы
  необнаружения CRC.
- Параметры кода настраиваются: `--cc-gens 171,133 --cc-K 7` даст rate 1/2,
  `--cc-gens 133,171,165,117` — rate 1/4 и т.д.
