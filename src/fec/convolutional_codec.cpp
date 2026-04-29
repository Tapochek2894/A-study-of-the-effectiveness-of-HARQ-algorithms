#include "fec/convolutional_codec.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace harq::fec {
namespace {

std::uint32_t Parity(std::uint32_t x) {
  x ^= x >> 16;
  x ^= x >> 8;
  x ^= x >> 4;
  x &= 0xFu;
  return (0x6996u >> x) & 1u;
}

int RequiredBits(std::uint32_t value) {
  int bits = 0;
  while (value != 0) {
    ++bits;
    value >>= 1u;
  }
  return bits;
}

void ValidateBitVector(const std::vector<std::uint8_t>& bits,
                       const char* name) {
  for (std::uint8_t bit : bits) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument(std::string(name) + " must contain only 0 and 1.");
    }
  }
}

}  // namespace

Trellis BuildTrellis(const std::vector<std::uint32_t>& generators) {
  if (generators.empty()) {
    throw std::invalid_argument("Convolutional generators must not be empty.");
  }

  const auto max_generator =
      *std::max_element(generators.begin(), generators.end());
  if (max_generator == 0) {
    throw std::invalid_argument("Convolutional generators must be non-zero.");
  }

  Trellis trellis;
  trellis.output_bits_per_symbol = static_cast<int>(generators.size());
  trellis.output_symbols = 1 << trellis.output_bits_per_symbol;
  trellis.constraint_length = RequiredBits(max_generator);
  const int memory = trellis.constraint_length - 1;
  trellis.states = 1 << memory;

  trellis.next_states.assign(trellis.states, std::vector<std::uint32_t>(2, 0));
  trellis.outputs.assign(trellis.states, std::vector<std::uint32_t>(2, 0));

  for (std::uint32_t state = 0; state < static_cast<std::uint32_t>(trellis.states); ++state) {
    for (std::uint32_t input = 0; input < 2; ++input) {
      const std::uint32_t reg = (input << memory) | state;

      std::uint32_t output_symbol = 0;
      for (std::uint32_t generator : generators) {
        const std::uint32_t bit = Parity(reg & generator);
        output_symbol = (output_symbol << 1u) | bit;
      }

      trellis.next_states[state][input] = reg >> 1u;
      trellis.outputs[state][input] = output_symbol;
    }
  }

  return trellis;
}

std::vector<std::uint8_t> ConvolutionalEncode(
    const Trellis& trellis,
    const std::vector<std::uint8_t>& input_bits) {
  ValidateBitVector(input_bits, "input_bits");

  std::vector<std::uint8_t> terminated_bits = input_bits;
  terminated_bits.insert(terminated_bits.end(),
                         static_cast<std::size_t>(trellis.constraint_length - 1),
                         0);

  std::vector<std::uint8_t> output;
  output.reserve(terminated_bits.size() *
                 static_cast<std::size_t>(trellis.output_bits_per_symbol));

  std::uint32_t state = 0;
  for (std::uint8_t input : terminated_bits) {
    const std::uint32_t output_symbol = trellis.outputs[state][input];
    state = trellis.next_states[state][input];

    for (int i = trellis.output_bits_per_symbol - 1; i >= 0; --i) {
      output.push_back(static_cast<std::uint8_t>((output_symbol >> i) & 1u));
    }
  }

  return output;
}

std::vector<std::uint8_t> HardViterbiDecode(
    const Trellis& trellis,
    const std::vector<std::uint8_t>& encoded_bits) {
  ValidateBitVector(encoded_bits, "encoded_bits");
  if (encoded_bits.size() % static_cast<std::size_t>(trellis.output_bits_per_symbol) != 0) {
    throw std::invalid_argument("encoded_bits size must be a multiple of code rate denominator.");
  }

  const int rate_den = trellis.output_bits_per_symbol;
  const int states = trellis.states;
  const std::size_t symbols = encoded_bits.size() / static_cast<std::size_t>(rate_den);

  std::vector<int> state_metric(states, std::numeric_limits<int>::max() / 4);
  state_metric[0] = 0;

  std::vector<std::vector<int>> traceback_state(states, std::vector<int>(symbols, 0));
  std::vector<std::vector<std::uint8_t>> traceback_input(
      states, std::vector<std::uint8_t>(symbols, 0));

  for (std::size_t i = 0; i < symbols; ++i) {
    std::uint32_t received_symbol = 0;
    for (int c = 0; c < rate_den; ++c) {
      received_symbol =
          (received_symbol << 1u) | encoded_bits[i * rate_den + c];
    }

    std::vector<int> next_metric(states, std::numeric_limits<int>::max() / 4);
    for (int state = 0; state < states; ++state) {
      for (std::uint8_t input = 0; input < 2; ++input) {
        const int next_state = static_cast<int>(trellis.next_states[state][input]);
        const std::uint32_t output_symbol = trellis.outputs[state][input];
        const int branch_metric = static_cast<int>(
            __builtin_popcount(received_symbol ^ output_symbol));
        const int candidate_metric = state_metric[state] + branch_metric;

        if (candidate_metric < next_metric[next_state]) {
          next_metric[next_state] = candidate_metric;
          traceback_state[next_state][i] = state;
          traceback_input[next_state][i] = input;
        }
      }
    }
    state_metric = std::move(next_metric);
  }

  std::vector<std::uint8_t> output(symbols, 0);
  int state = 0;
  for (std::size_t i = symbols; i > 0; --i) {
    output[i - 1] = traceback_input[state][i - 1];
    state = traceback_state[state][i - 1];
  }

  return output;
}

std::vector<std::uint8_t> SoftViterbiDecode(
    const Trellis& trellis,
    const std::vector<double>& soft_bits) {
  if (soft_bits.size() % static_cast<std::size_t>(trellis.output_bits_per_symbol) != 0) {
    throw std::invalid_argument("soft_bits size must be a multiple of code rate denominator.");
  }

  const int rate_den = trellis.output_bits_per_symbol;
  const int states = trellis.states;
  const std::size_t symbols = soft_bits.size() / static_cast<std::size_t>(rate_den);

  std::vector<double> state_metric(states, std::numeric_limits<double>::infinity());
  state_metric[0] = 0.0;

  std::vector<std::vector<int>> traceback_state(states, std::vector<int>(symbols, 0));
  std::vector<std::vector<std::uint8_t>> traceback_input(
      states, std::vector<std::uint8_t>(symbols, 0));

  for (std::size_t i = 0; i < symbols; ++i) {
    std::vector<double> next_metric(states, std::numeric_limits<double>::infinity());

    for (int state = 0; state < states; ++state) {
      if (!std::isfinite(state_metric[state])) {
        continue;
      }

      for (std::uint8_t input = 0; input < 2; ++input) {
        const int next_state = static_cast<int>(trellis.next_states[state][input]);
        const std::uint32_t output_symbol = trellis.outputs[state][input];

        double correlation = 0.0;
        for (int c = 0; c < rate_den; ++c) {
          const std::uint32_t bit =
              (output_symbol >> (rate_den - c - 1)) & 1u;
          const double expected = bit == 1u ? 1.0 : -1.0;
          correlation += expected * soft_bits[i * rate_den + c];
        }

        const double candidate_metric = state_metric[state] - correlation;
        if (candidate_metric < next_metric[next_state]) {
          next_metric[next_state] = candidate_metric;
          traceback_state[next_state][i] = state;
          traceback_input[next_state][i] = input;
        }
      }
    }
    state_metric = std::move(next_metric);
  }

  std::vector<std::uint8_t> output(symbols, 0);
  int state = 0;
  for (std::size_t i = symbols; i > 0; --i) {
    output[i - 1] = traceback_input[state][i - 1];
    state = traceback_state[state][i - 1];
  }

  return output;
}

ConvolutionalCodec::ConvolutionalCodec(const FecConfig& config)
    : input_bits_per_frame_(config.conv_input_bits_per_frame),
      output_bits_per_frame_(0),
      trellis_(BuildTrellis(config.conv_generators)) {
  if (input_bits_per_frame_ <= 0) {
    throw std::invalid_argument("conv_input_bits_per_frame must be positive.");
  }
  if (config.conv_rate_num != 1 ||
      config.conv_rate_den != trellis_.output_bits_per_symbol) {
    throw std::invalid_argument("ConvolutionalCodec expects rate 1 / generators.size().");
  }

  output_bits_per_frame_ =
      (input_bits_per_frame_ + trellis_.constraint_length - 1) *
      trellis_.output_bits_per_symbol;
}

int ConvolutionalCodec::input_bits_per_frame() const {
  return input_bits_per_frame_;
}

int ConvolutionalCodec::output_bits_per_frame() const {
  return output_bits_per_frame_;
}

std::vector<std::uint8_t> ConvolutionalCodec::Encode(
    const std::vector<std::uint8_t>& info_bits) const {
  if (static_cast<int>(info_bits.size()) != input_bits_per_frame_) {
    throw std::invalid_argument("info_bits size must equal input_bits_per_frame.");
  }

  return ConvolutionalEncode(trellis_, info_bits);
}

std::vector<std::uint8_t> ConvolutionalCodec::DecodeHard(
    const std::vector<std::uint8_t>& codeword_bits) const {
  if (static_cast<int>(codeword_bits.size()) != output_bits_per_frame_) {
    throw std::invalid_argument(
        "codeword_bits size must equal output_bits_per_frame.");
  }

  return StripTail(HardViterbiDecode(trellis_, codeword_bits));
}

std::vector<std::uint8_t> ConvolutionalCodec::DecodeSoft(
    const std::vector<double>& soft_bits) const {
  if (static_cast<int>(soft_bits.size()) != output_bits_per_frame_) {
    throw std::invalid_argument("soft_bits size must equal output_bits_per_frame.");
  }

  return StripTail(SoftViterbiDecode(trellis_, soft_bits));
}

std::vector<std::uint8_t> ConvolutionalCodec::StripTail(
    const std::vector<std::uint8_t>& decoded_with_tail) const {
  if (decoded_with_tail.size() < static_cast<std::size_t>(input_bits_per_frame_)) {
    throw std::invalid_argument("decoded sequence is shorter than the information frame.");
  }

  return std::vector<std::uint8_t>(
      decoded_with_tail.begin(),
      decoded_with_tail.begin() + input_bits_per_frame_);
}

}  // namespace harq::fec
