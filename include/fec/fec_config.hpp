#pragma once

namespace harq::fec {

enum class CodecType {
  kHamming,
  kConvolutionalAff3ct
};

struct FecConfig {
  CodecType codec_type = CodecType::kHamming;

  // Hamming parameters.
  int hamming_r = 3;
  bool hamming_extended = false;
};

}  // namespace harq::fec
