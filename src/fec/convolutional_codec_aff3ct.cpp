#include "fec/convolutional_codec_aff3ct.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace harq::fec {

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
