#pragma once

#include "fec/fec_config.hpp"
#include "fec/ifec_codec.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace harq::fec {

struct Trellis {
  int input_bits_per_symbol = 1;
  int input_symbols = 2;
  int output_bits_per_symbol = 0;
  int output_symbols = 0;
  int constraint_length = 0;
  int states = 0;

  std::vector<std::vector<std::uint32_t>> next_states;
  std::vector<std::vector<std::uint32_t>> outputs;
};

Trellis BuildTrellis(const std::vector<std::uint32_t>& generators);

std::vector<std::uint8_t> ConvolutionalEncode(
    const Trellis& trellis,
    const std::vector<std::uint8_t>& input_bits);

std::vector<std::uint8_t> HardViterbiDecode(
    const Trellis& trellis,
    const std::vector<std::uint8_t>& encoded_bits);

std::vector<std::uint8_t> SoftViterbiDecode(
    const Trellis& trellis,
    const std::vector<double>& soft_bits);

class ConvolutionalCodec final : public IFecCodec {
 public:
  explicit ConvolutionalCodec(const FecConfig& config);

  int input_bits_per_frame() const override;
  int output_bits_per_frame() const override;

  std::vector<std::uint8_t> Encode(
      const std::vector<std::uint8_t>& info_bits) const override;
  std::vector<std::uint8_t> DecodeHard(
      const std::vector<std::uint8_t>& codeword_bits) const override;
  std::vector<std::uint8_t> DecodeSoft(
      const std::vector<double>& soft_bits) const override;

 private:
  std::vector<std::uint8_t> StripTail(
      const std::vector<std::uint8_t>& decoded_with_tail) const;

  int input_bits_per_frame_;
  int output_bits_per_frame_;
  Trellis trellis_;
};

}  // namespace harq::fec
