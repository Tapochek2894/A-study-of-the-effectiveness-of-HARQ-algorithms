#pragma once

#include "fec/fec_config.hpp"
#include "fec/ifec_codec.hpp"

#include <cstdint>
#include <vector>

namespace harq::fec {

// Нативный несистематический свёрточный код (rate 1/n) с терминированием
// в нулевое состояние и декодированием по Витерби (hard и soft).
//
// База по умолчанию — классический код Оденвальдера (133,171,165)_8, K = 7,
// rate 1/3, свободное расстояние d_free = 15 (Proakis, "Digital
// Communications", таблицы оптимальных свёрточных кодов).
//
// Кодер: на каждый информационный бит выдаётся n = число генераторов
// выходных бит. После k информационных бит дописывается K-1 нулевых хвостовых
// бит, возвращающих регистр в нулевое состояние, поэтому
//   output_bits = n * (k + (K - 1)).
//
// Декодер: алгоритм Витерби по решётке из 2^(K-1) состояний. Метрика ветви
// для soft-входа — корреляция Σ (2c_i - 1)·L_i (L > 0 ⇒ бит '1', как в проекте),
// что эквивалентно ML-приёму в АБГШ. Для hard-входа биты отображаются в ±1,
// и максимизация корреляции эквивалентна минимизации расстояния Хэмминга.
class ConvolutionalCcCodec final : public IFecCodec {
 public:
  explicit ConvolutionalCcCodec(const FecConfig& config);

  int input_bits_per_frame() const override;
  int output_bits_per_frame() const override;

  std::vector<uint8_t> Encode(
      const std::vector<uint8_t>& info_bits) const override;
  std::vector<uint8_t> DecodeHard(
      const std::vector<uint8_t>& codeword_bits) const override;
  std::vector<uint8_t> DecodeSoft(
      const std::vector<double>& soft_bits) const override;

  int constraint_length() const { return constraint_length_; }
  int rate_den() const { return n_outputs_; }

 private:
  // Ядро декодера: на вход — «надёжности» каждого кодового бита
  // (L > 0 ⇒ бит '1'), на выход — k информационных бит.
  std::vector<uint8_t> Viterbi(const std::vector<double>& reliabilities) const;

  int input_bits_;
  int constraint_length_;  // K
  int memory_;             // m = K - 1
  int num_states_;         // 2^m
  int n_outputs_;          // знаменатель скорости (число генераторов)
  int output_bits_;        // n_outputs_ * (input_bits_ + memory_)
  std::vector<unsigned> generators_;  // K-битные маски ответвлений

  // Предвычисленная решётка: индекс [state * 2 + bit].
  std::vector<int> next_state_;
  std::vector<uint8_t> out_bits_;  // n_outputs_ бит подряд на каждый переход

  // Рабочие буферы декодера, выделяются один раз и переиспользуются между
  // вызовами (экземпляр кодека используется однопоточно). mutable — потому
  // что Decode*/Viterbi помечены const по контракту IFecCodec.
  mutable std::vector<double> vit_cur_;
  mutable std::vector<double> vit_nxt_;
  mutable std::vector<int> vit_prev_;
  mutable std::vector<uint8_t> vit_dec_;
};

}  // namespace harq::fec
