#include "fec/convolutional_codec_aff3ct.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace harq::fec {
namespace {

constexpr int kConvConstraintLength = 3;
constexpr int kConvStateCount = 1 << (kConvConstraintLength - 1);
constexpr double kInfMetric = 1e18;

std::vector<int> BuildMapping(const int target_size, const int mother_size) {
  std::vector<int> mapping;
  mapping.reserve(static_cast<std::size_t>(target_size));
  for (int i = 0; i < target_size; ++i) {
    const std::int64_t num = static_cast<std::int64_t>(i) * mother_size;
    int idx = static_cast<int>(num / target_size);
    if (idx < 0) idx = 0;
    if (idx >= mother_size) idx = mother_size - 1;
    mapping.push_back(idx);
  }
  return mapping;
}

inline std::array<uint8_t, 2> EncodeStep(const int state, const uint8_t input_bit) {
  const uint8_t prev1 = static_cast<uint8_t>((state >> 1) & 1);
  const uint8_t prev2 = static_cast<uint8_t>(state & 1);
  const uint8_t y0 = static_cast<uint8_t>(input_bit ^ prev1 ^ prev2);  // G0=111
  const uint8_t y1 = static_cast<uint8_t>(input_bit ^ prev2);          // G1=101
  return {y0, y1};
}

inline int NextState(const int state, const uint8_t input_bit) {
  const uint8_t prev1 = static_cast<uint8_t>((state >> 1) & 1);
  return (static_cast<int>(input_bit) << 1) | static_cast<int>(prev1);
}

std::vector<uint8_t> ViterbiDecodeFromLlr(const std::vector<double>& llr_pairs) {
  if (llr_pairs.size() % 2 != 0) {
    throw std::invalid_argument("Internal error: LLR vector size must be even.");
  }
  const int frame_bits = static_cast<int>(llr_pairs.size() / 2);
  if (frame_bits <= 0) {
    throw std::invalid_argument("Internal error: frame size must be positive.");
  }

  std::vector<double> metric_prev(kConvStateCount, kInfMetric);
  std::vector<double> metric_next(kConvStateCount, kInfMetric);
  metric_prev[0] = 0.0;  // Zero initial state.

  std::vector<std::array<uint8_t, kConvStateCount>> prev_state(
      static_cast<std::size_t>(frame_bits));
  std::vector<std::array<uint8_t, kConvStateCount>> prev_input(
      static_cast<std::size_t>(frame_bits));

  for (int t = 0; t < frame_bits; ++t) {
    std::fill(metric_next.begin(), metric_next.end(), kInfMetric);
    const double llr0 = llr_pairs[static_cast<std::size_t>(2 * t)];
    const double llr1 = llr_pairs[static_cast<std::size_t>(2 * t + 1)];

    for (int state = 0; state < kConvStateCount; ++state) {
      if (metric_prev[static_cast<std::size_t>(state)] >= kInfMetric / 2.0) {
        continue;
      }
      for (uint8_t input_bit = 0; input_bit <= 1; ++input_bit) {
        const auto out = EncodeStep(state, input_bit);
        const int ns = NextState(state, input_bit);

        const double m0 = (llr0 == 0.0) ? 0.0 : (out[0] ? -llr0 : llr0);
        const double m1 = (llr1 == 0.0) ? 0.0 : (out[1] ? -llr1 : llr1);
        const double candidate =
            metric_prev[static_cast<std::size_t>(state)] + m0 + m1;

        if (candidate < metric_next[static_cast<std::size_t>(ns)]) {
          metric_next[static_cast<std::size_t>(ns)] = candidate;
          prev_state[static_cast<std::size_t>(t)][static_cast<std::size_t>(ns)] =
              static_cast<uint8_t>(state);
          prev_input[static_cast<std::size_t>(t)][static_cast<std::size_t>(ns)] =
              input_bit;
        }
      }
    }
    metric_prev.swap(metric_next);
  }

  int best_state = 0;
  double best_metric = metric_prev[0];
  for (int s = 1; s < kConvStateCount; ++s) {
    if (metric_prev[static_cast<std::size_t>(s)] < best_metric) {
      best_metric = metric_prev[static_cast<std::size_t>(s)];
      best_state = s;
    }
  }

  std::vector<uint8_t> decoded(static_cast<std::size_t>(frame_bits), 0);
  int state = best_state;
  for (int t = frame_bits - 1; t >= 0; --t) {
    const uint8_t bit =
        prev_input[static_cast<std::size_t>(t)][static_cast<std::size_t>(state)];
    decoded[static_cast<std::size_t>(t)] = bit;
    state =
        prev_state[static_cast<std::size_t>(t)][static_cast<std::size_t>(state)];
  }
  return decoded;
}

}  // namespace

ConvolutionalCodecAff3ct::ConvolutionalCodecAff3ct(const FecConfig& config)
    : input_bits_per_frame_(config.conv_input_bits_per_frame),
      output_bits_per_frame_(0),
      rate_num_(config.conv_rate_num),
      rate_den_(config.conv_rate_den),
      decoder_type_(config.conv_decoder) {
  if (input_bits_per_frame_ <= 0) {
    throw std::invalid_argument("conv_input_bits_per_frame must be positive.");
  }
  if (rate_num_ <= 0 || rate_den_ <= 0) {
    throw std::invalid_argument("conv_rate numerator and denominator must be positive.");
  }
  if (rate_num_ > rate_den_) {
    throw std::invalid_argument("conv_rate must be <= 1 (num <= den).");
  }

  const std::int64_t in = static_cast<std::int64_t>(input_bits_per_frame_);
  const std::int64_t num = static_cast<std::int64_t>(rate_num_);
  const std::int64_t den = static_cast<std::int64_t>(rate_den_);
  // Ceil(in * den / num) for frame-level output size estimate.
  const std::int64_t out = (in * den + num - 1) / num;
  if (out <= 0 || out > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument("conv frame size overflow.");
  }
  output_bits_per_frame_ = static_cast<int>(out);
}

int ConvolutionalCodecAff3ct::input_bits_per_frame() const {
  return input_bits_per_frame_;
}

int ConvolutionalCodecAff3ct::output_bits_per_frame() const {
  return output_bits_per_frame_;
}

std::vector<uint8_t> ConvolutionalCodecAff3ct::Encode(
    const std::vector<uint8_t>& info_bits) const {
  if (static_cast<int>(info_bits.size()) != input_bits_per_frame_) {
    throw std::invalid_argument("info_bits size must match input_bits_per_frame.");
  }
  for (uint8_t bit : info_bits) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("info_bits must contain only 0/1 values.");
    }
  }

  const int mother_size = input_bits_per_frame_ * 2;
  std::vector<uint8_t> mother(static_cast<std::size_t>(mother_size), 0);

  int state = 0;
  for (int t = 0; t < input_bits_per_frame_; ++t) {
    const uint8_t u = info_bits[static_cast<std::size_t>(t)];
    const auto out = EncodeStep(state, u);
    mother[static_cast<std::size_t>(2 * t)] = out[0];
    mother[static_cast<std::size_t>(2 * t + 1)] = out[1];
    state = NextState(state, u);
  }

  const std::vector<int> mapping = BuildMapping(output_bits_per_frame_, mother_size);
  std::vector<uint8_t> coded(static_cast<std::size_t>(output_bits_per_frame_), 0);
  for (int i = 0; i < output_bits_per_frame_; ++i) {
    coded[static_cast<std::size_t>(i)] =
        mother[static_cast<std::size_t>(mapping[static_cast<std::size_t>(i)])];
  }
  return coded;
}

std::vector<uint8_t> ConvolutionalCodecAff3ct::DecodeHard(
    const std::vector<uint8_t>& codeword_bits) const {
  if (static_cast<int>(codeword_bits.size()) != output_bits_per_frame_) {
    throw std::invalid_argument(
        "codeword_bits size must match output_bits_per_frame.");
  }
  std::vector<double> soft;
  soft.reserve(codeword_bits.size());
  for (uint8_t bit : codeword_bits) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("codeword_bits must contain only 0/1 values.");
    }
    soft.push_back(bit == 1 ? 4.0 : -4.0);
  }
  return DecodeSoft(soft);
}

std::vector<uint8_t> ConvolutionalCodecAff3ct::DecodeSoft(
    const std::vector<double>& soft_bits) const {
  if (static_cast<int>(soft_bits.size()) != output_bits_per_frame_) {
    throw std::invalid_argument("soft_bits size must match output_bits_per_frame.");
  }

  const int mother_size = input_bits_per_frame_ * 2;
  const std::vector<int> mapping = BuildMapping(output_bits_per_frame_, mother_size);

  std::vector<double> llr_mother(static_cast<std::size_t>(mother_size), 0.0);
  std::vector<int> llr_count(static_cast<std::size_t>(mother_size), 0);
  for (int i = 0; i < output_bits_per_frame_; ++i) {
    const int idx = mapping[static_cast<std::size_t>(i)];
    llr_mother[static_cast<std::size_t>(idx)] +=
        soft_bits[static_cast<std::size_t>(i)];
    llr_count[static_cast<std::size_t>(idx)] += 1;
  }
  for (int i = 0; i < mother_size; ++i) {
    if (llr_count[static_cast<std::size_t>(i)] > 1) {
      llr_mother[static_cast<std::size_t>(i)] /=
          static_cast<double>(llr_count[static_cast<std::size_t>(i)]);
    }
  }

  if (decoder_type_ == ConvDecoderType::kBcjr) {
    // BCJR is not implemented in this lightweight backend yet.
    // Fallback to Viterbi to keep CLI compatibility.
  }
  return ViterbiDecodeFromLlr(llr_mother);
}

}  // namespace harq::fec
