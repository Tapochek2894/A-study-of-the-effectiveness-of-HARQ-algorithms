#include "fec/hamming_codec.hpp"

#include <stdexcept>

namespace harq::fec {

HammingCodec::HammingCodec(int r, bool extended)
    : encoder_(r), decoder_(r), extended_(extended) {}

int HammingCodec::input_bits_per_frame() const { return encoder_.k(); }

int HammingCodec::output_bits_per_frame() const {
  return extended_ ? (encoder_.n() + 1) : encoder_.n();
}

std::vector<uint8_t> HammingCodec::Encode(
    const std::vector<uint8_t>& info_bits) const {
  return extended_ ? encoder_.EncodeExtended(info_bits)
                   : encoder_.Encode(info_bits);
}

std::vector<uint8_t> HammingCodec::DecodeHard(
    const std::vector<uint8_t>& codeword_bits) const {
  return decoder_.Decode(codeword_bits);
}

std::vector<uint8_t> HammingCodec::DecodeSoft(
    const std::vector<double>& soft_bits) const {
  if (soft_bits.empty()) {
    throw std::invalid_argument("soft_bits cannot be empty.");
  }

  std::vector<uint8_t> hard_bits;
  hard_bits.reserve(soft_bits.size());
  for (double v : soft_bits) {
    hard_bits.push_back(v >= 0.0 ? 1 : 0);
  }

  return decoder_.Decode(hard_bits);
}

}  // namespace harq::fec
