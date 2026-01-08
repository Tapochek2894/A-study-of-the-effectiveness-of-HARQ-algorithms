#include "hamming_encoder.hpp"

#include <stdexcept>

namespace harq {

HammingEncoder::HammingEncoder(int r) : r_(r), n_(0), k_(0) {
  if (r_ < 2) {
    throw std::invalid_argument("Hamming encoder expects r >= 2.");
  }

  n_ = (1 << r_) - 1;
  k_ = n_ - r_;

  parity_positions_.reserve(r_);
  for (int i = 0; i < r_; i++) {
    parity_positions_.push_back(1 << i);
  }

  for (int pos = 1; pos <= n_; pos++) {
    if (!IsPowerOfTwo(pos)) {
      data_positions_.push_back(pos);
    }
  }

  // Строим матрицу G, кодируя единичные векторы.
  generator_.resize(k_, std::vector<uint8_t>(n_, 0));
  for (int i = 0; i < k_; i++) {
    std::vector<uint8_t> basis(k_, 0);
    basis[i] = 1;
    generator_[i] = BuildCodewordFromData(basis);
  }
}

int HammingEncoder::n() const { return n_; }

int HammingEncoder::k() const { return k_; }

const std::vector<std::vector<uint8_t>>&
HammingEncoder::generator_matrix() const {
  return generator_;
}

std::vector<uint8_t> HammingEncoder::Encode(
    const std::vector<uint8_t>& data) const {
  if (static_cast<int>(data.size()) != k_) {
    throw std::invalid_argument("Hamming encoder expects k data bits.");
  }

  std::vector<uint8_t> codeword(n_, 0);
  // Перемножение сообщения с матрицей G над GF(2).
  for (int i = 0; i < k_; i++) {
    uint8_t bit = data[i];
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("Hamming encoder expects bits 0 or 1.");
    }
    if (bit == 1) {
      for (int j = 0; j < n_; j++) {
        codeword[j] ^= generator_[i][j];
      }
    }
  }

  return codeword;
}

std::vector<uint8_t> HammingEncoder::EncodeExtended(
    const std::vector<uint8_t>& data) const {
  std::vector<uint8_t> codeword = Encode(data);
  uint8_t parity = 0;
  for (uint8_t bit : codeword) {
    parity ^= bit;
  }
  codeword.push_back(parity);
  return codeword;
}

bool HammingEncoder::IsPowerOfTwo(int value) {
  return value > 0 && (value & (value - 1)) == 0;
}

std::vector<uint8_t> HammingEncoder::BuildCodewordFromData(
    const std::vector<uint8_t>& data) const {
  if (static_cast<int>(data.size()) != k_) {
    throw std::invalid_argument("Hamming encoder expects k data bits.");
  }

  std::vector<uint8_t> codeword(n_, 0);
  // Раскладываем данные по позициям, не являющимся паритетными.
  for (size_t i = 0; i < data_positions_.size(); i++) {
    uint8_t bit = data[i];
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("Hamming encoder expects bits 0 or 1.");
    }
    int pos = data_positions_[i];
    codeword[pos - 1] = bit;
  }

  // Считаем паритет как XOR по позициям с соответствующим установленным битом маски.
  for (int parity_pos : parity_positions_) {
    uint8_t parity = 0;
    for (int pos = 1; pos <= n_; pos++) {
      if (pos == parity_pos) {
        continue;
      }
      if ((pos & parity_pos) != 0) {
        parity ^= codeword[pos - 1];
      }
    }
    codeword[parity_pos - 1] = parity;
  }

  return codeword;
}

}  // namespace harq
