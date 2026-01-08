#include "hamming_decoder.hpp"

#include <stdexcept>

namespace harq {

HammingDecoder::HammingDecoder(int r) : r_(r), n_(0), k_(0) {
  if (r_ < 2) {
    throw std::invalid_argument("Hamming decoder expects r >= 2.");
  }

  n_ = (1 << r_) - 1;
  k_ = n_ - r_;

  for (int pos = 1; pos <= n_; pos++) {
    if (!IsPowerOfTwo(pos)) {
      data_positions_.push_back(pos);
    }
  }
}

int HammingDecoder::n() const { return n_; }

int HammingDecoder::k() const { return k_; }

std::vector<uint8_t> HammingDecoder::Correct(
    const std::vector<uint8_t>& codeword) const {
  if (static_cast<int>(codeword.size()) != n_) {
    throw std::invalid_argument("Hamming decoder expects n codeword bits.");
  }

  for (uint8_t bit : codeword) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("Hamming decoder expects bits 0 or 1.");
    }
  }

  std::vector<uint8_t> corrected = codeword;
  int syndrome = ComputeSyndrome(corrected);
  if (syndrome > 0 && syndrome <= n_) {
    corrected[syndrome - 1] ^= 1;
  }

  return corrected;
}

std::vector<uint8_t> HammingDecoder::Decode(
    const std::vector<uint8_t>& codeword) const {
  std::vector<uint8_t> corrected = Correct(codeword);

  std::vector<uint8_t> data;
  data.reserve(k_);
  for (int pos : data_positions_) {
    data.push_back(corrected[pos - 1]);
  }

  return data;
}

bool HammingDecoder::IsPowerOfTwo(int value) {
  return value > 0 && (value & (value - 1)) == 0;
}

int HammingDecoder::ComputeSyndrome(
    const std::vector<uint8_t>& codeword) const {
  int syndrome = 0;
  // Синдром соответствует XOR индексов позиций с единичными битами.
  for (int pos = 1; pos <= n_; pos++) {
    if (codeword[pos - 1] == 1) {
      syndrome ^= pos;
    }
  }
  return syndrome;
}

}  // namespace harq
