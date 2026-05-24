#pragma once

#include <cstdint>
#include <vector>

namespace harq::fec {

enum class CodecType {
  kHamming,
  kConvolutionalAff3ct,
  // Нативный несистематический свёрточный код (rate 1/n) с декодером Витерби.
  kConvolutionalCc
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
  std::uint32_t conv_seed = 5489u;
  ConvDecoderType conv_decoder = ConvDecoderType::kViterbi;

  // Нативный свёрточный код (kConvolutionalCc).
  // rate = 1 / cc_generators_octal.size(); терминирование в нуль хвостом
  // длины cc_constraint_length - 1. По умолчанию — классический код
  // Оденвальдера (133,171,165)_8, K=7, rate 1/3, d_free = 15.
  int cc_input_bits_per_frame = 512;
  int cc_constraint_length = 7;  // K (длина кодового ограничения), память = K-1.
  std::vector<unsigned> cc_generators_octal = {0133u, 0171u, 0165u};
};

}  // namespace harq::fec
