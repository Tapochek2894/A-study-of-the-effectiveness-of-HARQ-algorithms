#pragma once

#include "fec/fec_config.hpp"
#include "fec/ifec_codec.hpp"

#include <memory>

#if HARQ_ENABLE_AFF3CT
namespace aff3ct::tools {
template <typename B, typename Q>
class Codec_RSC;
}  // namespace aff3ct::tools
#endif

namespace harq::fec {

class ConvolutionalCodecAff3ct final : public IFecCodec {
 public:
  explicit ConvolutionalCodecAff3ct(const FecConfig& config);
  ~ConvolutionalCodecAff3ct() override;

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
  int rate_num_;
  int rate_den_;
  ConvDecoderType decoder_type_;

#if HARQ_ENABLE_AFF3CT
  mutable std::unique_ptr<aff3ct::tools::Codec_RSC<int, float>> codec_;
#endif
};

}  // namespace harq::fec
