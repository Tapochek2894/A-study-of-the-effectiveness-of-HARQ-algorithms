#pragma once

#include "fec/fec_config.hpp"
#include "fec/ifec_codec.hpp"

namespace harq::fec {

class ConvolutionalCodecAff3ct final : public IFecCodec {
 public:
  explicit ConvolutionalCodecAff3ct(const FecConfig& config);

  int input_bits_per_frame() const override;
  int output_bits_per_frame() const override;

  std::vector<uint8_t> Encode(
      const std::vector<uint8_t>& info_bits) const override;
  std::vector<uint8_t> DecodeHard(
      const std::vector<uint8_t>& codeword_bits) const override;
  std::vector<uint8_t> DecodeSoft(
      const std::vector<double>& soft_bits) const override;

 private:
  int input_bits_per_frame_;
  int output_bits_per_frame_;
  ConvDecoderType decoder_type_;
};

}  // namespace harq::fec
