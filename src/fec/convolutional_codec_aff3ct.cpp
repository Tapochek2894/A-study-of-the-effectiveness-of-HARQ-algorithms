#include "fec/convolutional_codec_aff3ct.hpp"

#include <stdexcept>

namespace harq::fec {

ConvolutionalCodecAff3ct::ConvolutionalCodecAff3ct(const FecConfig& config)
    : input_bits_per_frame_(config.conv_input_bits_per_frame),
      output_bits_per_frame_(config.conv_input_bits_per_frame * 2),
      decoder_type_(config.conv_decoder) {
  if (input_bits_per_frame_ <= 0) {
    throw std::invalid_argument("conv_input_bits_per_frame must be positive.");
  }
}

int ConvolutionalCodecAff3ct::input_bits_per_frame() const {
  return input_bits_per_frame_;
}

int ConvolutionalCodecAff3ct::output_bits_per_frame() const {
  return output_bits_per_frame_;
}

std::vector<uint8_t> ConvolutionalCodecAff3ct::Encode(
    const std::vector<uint8_t>& info_bits) const {
#if HARQ_ENABLE_AFF3CT
  (void)decoder_type_;
  (void)info_bits;
  throw std::invalid_argument(
      "Convolutional AFF3CT backend is enabled in build but not implemented yet.");
#else
  (void)decoder_type_;
  (void)info_bits;
  throw std::invalid_argument(
      "Convolutional AFF3CT backend is unavailable. Rebuild with ENABLE_AFF3CT=ON and installed AFF3CT.");
#endif
}

std::vector<uint8_t> ConvolutionalCodecAff3ct::DecodeHard(
    const std::vector<uint8_t>& codeword_bits) const {
#if HARQ_ENABLE_AFF3CT
  (void)codeword_bits;
  throw std::invalid_argument(
      "Convolutional AFF3CT backend is enabled in build but not implemented yet.");
#else
  (void)codeword_bits;
  throw std::invalid_argument(
      "Convolutional AFF3CT backend is unavailable. Rebuild with ENABLE_AFF3CT=ON and installed AFF3CT.");
#endif
}

std::vector<uint8_t> ConvolutionalCodecAff3ct::DecodeSoft(
    const std::vector<double>& soft_bits) const {
#if HARQ_ENABLE_AFF3CT
  (void)soft_bits;
  throw std::invalid_argument(
      "Convolutional AFF3CT backend is enabled in build but not implemented yet.");
#else
  (void)soft_bits;
  throw std::invalid_argument(
      "Convolutional AFF3CT backend is unavailable. Rebuild with ENABLE_AFF3CT=ON and installed AFF3CT.");
#endif
}

}  // namespace harq::fec
