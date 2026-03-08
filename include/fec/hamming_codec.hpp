#pragma once

#include "fec/ifec_codec.hpp"

#include "hamming_decoder.hpp"
#include "hamming_encoder.hpp"

namespace harq::fec {

class HammingCodec final : public IFecCodec {
 public:
  explicit HammingCodec(int r, bool extended = false);

  int input_bits_per_frame() const override;
  int output_bits_per_frame() const override;

  std::vector<uint8_t> Encode(
      const std::vector<uint8_t>& info_bits) const override;
  std::vector<uint8_t> DecodeHard(
      const std::vector<uint8_t>& codeword_bits) const override;
  std::vector<uint8_t> DecodeSoft(
      const std::vector<double>& soft_bits) const override;

 private:
  HammingEncoder encoder_;
  HammingDecoder decoder_;
  bool extended_;
};

}  // namespace harq::fec
