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
  if (static_cast<int>(codeword.size()) != n_ &&
      static_cast<int>(codeword.size()) != n_ + 1) {
    throw std::invalid_argument(
        "Hamming decoder expects n or n+1 codeword bits.");
  }

  for (uint8_t bit : codeword) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("Hamming decoder expects bits 0 or 1.");
    }
  }

  std::vector<uint8_t> corrected = codeword;

  bool extended = static_cast<int>(codeword.size()) == n_ + 1;
  int syndrome = ComputeSyndrome(corrected);

  if (extended) {
    uint8_t overall_parity = 0;
    for (uint8_t bit : corrected) {
      overall_parity ^= bit;
    }

    if (syndrome == 0 && overall_parity == 1) {
      corrected.back() ^= 1;
    } else if (syndrome != 0 && overall_parity == 1) {
      corrected[syndrome - 1] ^= 1;
    }
  } else if (syndrome > 0 && syndrome <= n_) {
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
  int limit = std::min(static_cast<int>(codeword.size()), n_);
  for (int pos = 1; pos <= limit; pos++) {
    if (codeword[pos - 1] == 1) {
      syndrome ^= pos;
    }
  }
  return syndrome;
}

std::pair<std::vector<uint8_t>, HammingDecoder::DecodeStatus>
HammingDecoder::DecodeWithStatus(const std::vector<uint8_t>& codeword) const {
  if (static_cast<int>(codeword.size()) != n_ &&
      static_cast<int>(codeword.size()) != n_ + 1) {
    throw std::invalid_argument(
        "Hamming decoder expects n or n+1 codeword bits.");
  }

  for (uint8_t bit : codeword) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("Hamming decoder expects bits 0 or 1.");
    }
  }

  bool extended = static_cast<int>(codeword.size()) == n_ + 1;
  std::vector<uint8_t> corrected = codeword;
  int syndrome = ComputeSyndrome(corrected);

  DecodeStatus status = DecodeStatus::kNoError;

  if (extended) {
    uint8_t overall_parity = 0;
    for (uint8_t bit : corrected) {
      overall_parity ^= bit;
    }

    if (syndrome == 0 && overall_parity == 0) {
      status = DecodeStatus::kNoError;
    } else if (syndrome != 0 && overall_parity == 1) {
      corrected[syndrome - 1] ^= 1;
      status = DecodeStatus::kCorrected;
    } else if (syndrome == 0 && overall_parity == 1) {
      corrected.back() ^= 1;
      status = DecodeStatus::kParityCorrected;
    } else {
      status = DecodeStatus::kDetectedDouble;
    }
  } else {
    if (syndrome > 0 && syndrome <= n_) {
      corrected[syndrome - 1] ^= 1;
      status = DecodeStatus::kCorrected;
    } else {
      status = DecodeStatus::kNoError;
    }
  }

  std::vector<uint8_t> data;
  data.reserve(k_);
  for (int pos : data_positions_) {
    data.push_back(corrected[pos - 1]);
  }

  return {data, status};
}

}  // namespace harq
