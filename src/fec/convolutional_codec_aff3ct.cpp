#include "fec/convolutional_codec_aff3ct.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#if HARQ_ENABLE_AFF3CT
#include <Factory/Module/Decoder/RSC/Decoder_RSC.hpp>
#include <Factory/Module/Encoder/RSC/Encoder_RSC.hpp>
#include <Factory/Tools/Codec/RSC/Codec_RSC.hpp>
#include <Tools/Codec/RSC/Codec_RSC.hpp>
#endif

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

#if HARQ_ENABLE_AFF3CT
  aff3ct::factory::Codec_RSC codec_params;

  auto* enc_params =
      dynamic_cast<aff3ct::factory::Encoder_RSC*>(codec_params.enc.get());
  auto* dec_params =
      dynamic_cast<aff3ct::factory::Decoder_RSC*>(codec_params.dec.get());
  if (enc_params == nullptr || dec_params == nullptr) {
    throw std::invalid_argument(
        "Failed to initialize AFF3CT RSC codec parameters.");
  }

  // AFF3CT's default RSC setup is a practical baseline for convolutional coding.
  // The actual frame size is fully defined by K and the chosen polynomials/tail.
  enc_params->K = input_bits_per_frame_;
  enc_params->buffered = (decoder_type_ == ConvDecoderType::kBcjr);
  enc_params->tail_length = 6;
  enc_params->N_cw = 2 * enc_params->K + enc_params->tail_length;
  enc_params->R = static_cast<float>(enc_params->K) /
                  static_cast<float>(enc_params->N_cw);

  dec_params->K = enc_params->K;
  dec_params->N_cw = enc_params->N_cw;
  dec_params->tail_length = enc_params->tail_length;
  dec_params->buffered = enc_params->buffered;
  dec_params->poly = enc_params->poly;
  dec_params->standard = enc_params->standard;
  dec_params->type = decoder_type_ == ConvDecoderType::kViterbi ? "VITERBI"
                                                                 : "BCJR";
  dec_params->implem = "STD";

  codec_.reset(codec_params.build<int, float>(nullptr));
  if (!codec_) {
    throw std::invalid_argument("AFF3CT codec build failed.");
  }

  input_bits_per_frame_ = codec_->get_encoder().get_K();
  output_bits_per_frame_ = codec_->get_encoder().get_N();
#endif
}

ConvolutionalCodecAff3ct::~ConvolutionalCodecAff3ct() = default;

int ConvolutionalCodecAff3ct::input_bits_per_frame() const {
  return input_bits_per_frame_;
}

int ConvolutionalCodecAff3ct::output_bits_per_frame() const {
  return output_bits_per_frame_;
}

std::vector<uint8_t> ConvolutionalCodecAff3ct::Encode(
    const std::vector<uint8_t>& info_bits) const {
#if HARQ_ENABLE_AFF3CT
  if (static_cast<int>(info_bits.size()) != input_bits_per_frame_) {
    throw std::invalid_argument("info_bits size must equal input_bits_per_frame.");
  }

  std::vector<int> input(static_cast<std::size_t>(input_bits_per_frame_), 0);
  for (int i = 0; i < input_bits_per_frame_; ++i) {
    const uint8_t bit = info_bits[static_cast<std::size_t>(i)];
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("info_bits must contain only 0 and 1.");
    }
    input[static_cast<std::size_t>(i)] = static_cast<int>(bit);
  }

  std::vector<int> codeword(static_cast<std::size_t>(output_bits_per_frame_), 0);
  codec_->get_encoder().encode(input, codeword);

  std::vector<uint8_t> out(static_cast<std::size_t>(output_bits_per_frame_), 0);
  for (int i = 0; i < output_bits_per_frame_; ++i) {
    out[static_cast<std::size_t>(i)] =
        static_cast<uint8_t>(codeword[static_cast<std::size_t>(i)] != 0);
  }
  return out;
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
  if (static_cast<int>(codeword_bits.size()) != output_bits_per_frame_) {
    throw std::invalid_argument(
        "codeword_bits size must equal output_bits_per_frame.");
  }

  std::vector<int> input(static_cast<std::size_t>(output_bits_per_frame_), 0);
  for (int i = 0; i < output_bits_per_frame_; ++i) {
    const uint8_t bit = codeword_bits[static_cast<std::size_t>(i)];
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("codeword_bits must contain only 0 and 1.");
    }
    input[static_cast<std::size_t>(i)] = static_cast<int>(bit);
  }

  std::vector<int> decoded(static_cast<std::size_t>(input_bits_per_frame_), 0);
  codec_->get_decoder_siho().decode_hiho(input, decoded);

  std::vector<uint8_t> out(static_cast<std::size_t>(input_bits_per_frame_), 0);
  for (int i = 0; i < input_bits_per_frame_; ++i) {
    out[static_cast<std::size_t>(i)] =
        static_cast<uint8_t>(decoded[static_cast<std::size_t>(i)] != 0);
  }
  return out;
#else
  (void)codeword_bits;
  throw std::invalid_argument(
      "Convolutional AFF3CT backend is unavailable. Rebuild with ENABLE_AFF3CT=ON and installed AFF3CT.");
#endif
}

std::vector<uint8_t> ConvolutionalCodecAff3ct::DecodeSoft(
    const std::vector<double>& soft_bits) const {
#if HARQ_ENABLE_AFF3CT
  if (static_cast<int>(soft_bits.size()) != output_bits_per_frame_) {
    throw std::invalid_argument("soft_bits size must equal output_bits_per_frame.");
  }

  std::vector<float> input(static_cast<std::size_t>(output_bits_per_frame_), 0.0f);
  for (int i = 0; i < output_bits_per_frame_; ++i) {
    // Project convention: positive reliability -> bit '1'.
    // AFF3CT decoder convention: positive LLR -> bit '0'.
    input[static_cast<std::size_t>(i)] =
        static_cast<float>(-soft_bits[static_cast<std::size_t>(i)]);
  }

  std::vector<int> decoded(static_cast<std::size_t>(input_bits_per_frame_), 0);
  codec_->get_decoder_siho().decode_siho(input, decoded);

  std::vector<uint8_t> out(static_cast<std::size_t>(input_bits_per_frame_), 0);
  for (int i = 0; i < input_bits_per_frame_; ++i) {
    out[static_cast<std::size_t>(i)] =
        static_cast<uint8_t>(decoded[static_cast<std::size_t>(i)] != 0);
  }
  return out;
#else
  (void)soft_bits;
  throw std::invalid_argument(
      "Convolutional AFF3CT backend is unavailable. Rebuild with ENABLE_AFF3CT=ON and installed AFF3CT.");
#endif
}

}  // namespace harq::fec
