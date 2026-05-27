#include "fec/convolutional_cc_codec.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace harq::fec {

namespace {

// Чётность числа установленных бит (GF(2) свёртка по маске генератора).
inline int Parity(unsigned x) {
  return __builtin_parity(x);
}

}  // namespace

ConvolutionalCcCodec::ConvolutionalCcCodec(const FecConfig& config)
    : input_bits_(config.cc_input_bits_per_frame),
      constraint_length_(config.cc_constraint_length),
      memory_(config.cc_constraint_length - 1),
      num_states_(0),
      n_outputs_(static_cast<int>(config.cc_generators_octal.size())),
      output_bits_(0),
      generators_(config.cc_generators_octal) {
  if (input_bits_ <= 0) {
    throw std::invalid_argument("cc_input_bits_per_frame must be positive.");
  }
  if (constraint_length_ < 2 || constraint_length_ > 16) {
    throw std::invalid_argument("cc_constraint_length must be in [2, 16].");
  }
  if (n_outputs_ < 2) {
    throw std::invalid_argument("cc requires at least 2 generator polynomials.");
  }
  const unsigned max_gen = (1u << constraint_length_) - 1u;
  for (unsigned g : generators_) {
    if (g == 0u || g > max_gen) {
      throw std::invalid_argument(
          "cc generator must be nonzero and fit in constraint_length bits.");
    }
  }

  num_states_ = 1 << memory_;
  output_bits_ = n_outputs_ * (input_bits_ + memory_);

  // Предвычисляем переходы решётки и выходные биты для каждого (state, bit).
  next_state_.assign(static_cast<std::size_t>(num_states_) * 2u, 0);
  out_bits_.assign(
      static_cast<std::size_t>(num_states_) * 2u *
          static_cast<std::size_t>(n_outputs_),
      0);
  const unsigned state_mask = static_cast<unsigned>(num_states_ - 1);
  for (int s = 0; s < num_states_; ++s) {
    for (int b = 0; b < 2; ++b) {
      // Окно K бит: текущий вход в старшем разряде, далее регистр состояния.
      const unsigned window =
          (static_cast<unsigned>(b) << memory_) | static_cast<unsigned>(s);
      const int idx = s * 2 + b;
      next_state_[static_cast<std::size_t>(idx)] =
          static_cast<int>((window >> 1) & state_mask);
      for (int j = 0; j < n_outputs_; ++j) {
        out_bits_[static_cast<std::size_t>(idx) *
                      static_cast<std::size_t>(n_outputs_) +
                  static_cast<std::size_t>(j)] =
            static_cast<uint8_t>(Parity(window & generators_[static_cast<std::size_t>(j)]));
      }
    }
  }

  // Буферы декодера Витерби — один раз на весь срок жизни кодека.
  const int total_steps = input_bits_ + memory_;
  vit_cur_.resize(static_cast<std::size_t>(num_states_));
  vit_nxt_.resize(static_cast<std::size_t>(num_states_));
  vit_prev_.resize(static_cast<std::size_t>(total_steps) *
                   static_cast<std::size_t>(num_states_));
  vit_dec_.resize(static_cast<std::size_t>(total_steps) *
                  static_cast<std::size_t>(num_states_));
}

int ConvolutionalCcCodec::input_bits_per_frame() const { return input_bits_; }
int ConvolutionalCcCodec::output_bits_per_frame() const { return output_bits_; }

std::vector<uint8_t> ConvolutionalCcCodec::Encode(
    const std::vector<uint8_t>& info_bits) const {
  if (static_cast<int>(info_bits.size()) != input_bits_) {
    throw std::invalid_argument("info_bits size must equal input_bits_per_frame.");
  }
  std::vector<uint8_t> out;
  out.reserve(static_cast<std::size_t>(output_bits_));

  const int total_steps = input_bits_ + memory_;
  int state = 0;
  for (int t = 0; t < total_steps; ++t) {
    int b;
    if (t < input_bits_) {
      b = info_bits[static_cast<std::size_t>(t)];
      if (b != 0 && b != 1) {
        throw std::invalid_argument("info_bits must contain only 0 and 1.");
      }
    } else {
      b = 0;  // хвостовые биты терминирования
    }
    const int idx = state * 2 + b;
    for (int j = 0; j < n_outputs_; ++j) {
      out.push_back(out_bits_[static_cast<std::size_t>(idx) *
                                  static_cast<std::size_t>(n_outputs_) +
                              static_cast<std::size_t>(j)]);
    }
    state = next_state_[static_cast<std::size_t>(idx)];
  }
  return out;
}

std::vector<uint8_t> ConvolutionalCcCodec::Viterbi(
    const std::vector<double>& reliabilities) const {
  const int total_steps = input_bits_ + memory_;
  constexpr double kNegInf = -std::numeric_limits<double>::infinity();

  // Переиспользуем буферы кодека. Метрики cur инициализируем заново; prev/dec
  // перезаписываются только для достижимых состояний, а обратный проход идёт
  // лишь по достижимым ячейкам, поэтому очищать их целиком не нужно.
  std::vector<double>& cur = vit_cur_;
  std::vector<double>& nxt = vit_nxt_;
  std::vector<int>& prev = vit_prev_;
  std::vector<uint8_t>& dec = vit_dec_;
  std::fill(cur.begin(), cur.end(), kNegInf);
  cur[0] = 0.0;  // кодер стартует из нулевого состояния

  for (int t = 0; t < total_steps; ++t) {
    std::fill(nxt.begin(), nxt.end(), kNegInf);
    // На хвостовых шагах вход принудительно равен 0.
    const int b_max = (t < input_bits_) ? 1 : 0;
    const double* l = &reliabilities[static_cast<std::size_t>(t) *
                                     static_cast<std::size_t>(n_outputs_)];

    for (int s = 0; s < num_states_; ++s) {
      const double pm = cur[static_cast<std::size_t>(s)];
      if (pm == kNegInf) continue;
      for (int b = 0; b <= b_max; ++b) {
        const int idx = s * 2 + b;
        const uint8_t* ob = &out_bits_[static_cast<std::size_t>(idx) *
                                       static_cast<std::size_t>(n_outputs_)];
        double branch = 0.0;
        for (int j = 0; j < n_outputs_; ++j) {
          // (2c - 1)·L: складываем L при c=1, вычитаем при c=0.
          branch += (ob[j] ? l[j] : -l[j]);
        }
        const int ns = next_state_[static_cast<std::size_t>(idx)];
        const double cand = pm + branch;
        if (cand > nxt[static_cast<std::size_t>(ns)]) {
          nxt[static_cast<std::size_t>(ns)] = cand;
          const std::size_t cell = static_cast<std::size_t>(t) *
                                       static_cast<std::size_t>(num_states_) +
                                   static_cast<std::size_t>(ns);
          prev[cell] = s;
          dec[cell] = static_cast<uint8_t>(b);
        }
      }
    }
    cur.swap(nxt);
  }

  // Терминирование гарантирует завершение в состоянии 0.
  std::vector<uint8_t> info(static_cast<std::size_t>(input_bits_), 0);
  int state = 0;
  for (int t = total_steps - 1; t >= 0; --t) {
    const std::size_t cell = static_cast<std::size_t>(t) *
                                 static_cast<std::size_t>(num_states_) +
                             static_cast<std::size_t>(state);
    const uint8_t b = dec[cell];
    if (t < input_bits_) {
      info[static_cast<std::size_t>(t)] = b;
    }
    state = prev[cell];
  }
  return info;
}

std::vector<uint8_t> ConvolutionalCcCodec::DecodeSoft(
    const std::vector<double>& soft_bits) const {
  if (static_cast<int>(soft_bits.size()) != output_bits_) {
    throw std::invalid_argument("soft_bits size must equal output_bits_per_frame.");
  }
  return Viterbi(soft_bits);
}

std::vector<uint8_t> ConvolutionalCcCodec::DecodeHard(
    const std::vector<uint8_t>& codeword_bits) const {
  if (static_cast<int>(codeword_bits.size()) != output_bits_) {
    throw std::invalid_argument(
        "codeword_bits size must equal output_bits_per_frame.");
  }
  // Жёсткие биты → ±1; максимизация корреляции = минимизация расстояния Хэмминга.
  std::vector<double> reliabilities(static_cast<std::size_t>(output_bits_), 0.0);
  for (int i = 0; i < output_bits_; ++i) {
    const uint8_t bit = codeword_bits[static_cast<std::size_t>(i)];
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("codeword_bits must contain only 0 and 1.");
    }
    reliabilities[static_cast<std::size_t>(i)] = bit ? 1.0 : -1.0;
  }
  return Viterbi(reliabilities);
}

}  // namespace harq::fec
