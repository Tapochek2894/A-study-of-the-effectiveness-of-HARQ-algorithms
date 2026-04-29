#pragma once

#include <cstdint>
#include <vector>

namespace harq::fec {

enum class CodecType {
  kHamming,
  kConvolutionalAff3ct,
  kConvolutional
};

enum class ConvDecoderType {
  kViterbi,
  kBcjr
};

struct FecConfig {
  CodecType codec_type = CodecType::kHamming;

  // Hamming parameters.
  int hamming_r = 3;
  bool hamming_extended = false;

  // Convolutional (AFF3CT) parameters (frame-based contract).
  int conv_input_bits_per_frame = 1024;
  int conv_rate_num = 1;
  int conv_rate_den = 2;
  std::vector<std::uint32_t> conv_generators = {0133, 0171, 0165};
  std::uint32_t conv_seed = 5489u;
  ConvDecoderType conv_decoder = ConvDecoderType::kViterbi;
};

}  // namespace harq::fec
