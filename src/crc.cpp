#include "crc.hpp"

#include <algorithm>
#include <stdexcept>

namespace harq {

Crc::Crc(const std::vector<uint8_t>& polynomial) : polynomial_(polynomial) {
  if (polynomial_.size() < 2) {
    throw std::invalid_argument(
        "CRC polynomial must have at least 2 coefficients.");
  }

  if (polynomial_[0] != 1) {
    throw std::invalid_argument(
        "CRC polynomial must have leading coefficient 1.");
  }

  for (const auto& bit : polynomial_) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument(
          "CRC polynomial coefficients must be 0 or 1.");
    }
  }

  r_ = static_cast<int>(polynomial_.size()) - 1;
}

int Crc::r() const { return r_; }

const std::vector<uint8_t>& Crc::polynomial() const { return polynomial_; }

std::vector<uint8_t> Crc::Encode(const std::vector<uint8_t>& data) const {
  if (data.empty()) {
    throw std::invalid_argument("Input data cannot be empty.");
  }

  for (const auto& bit : data) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("Input data must contain only 0 and 1.");
    }
  }

  // Шаг 1-2: Формируем m(x) * x^r (сдвиг на r позиций)
  std::vector<uint8_t> shifted(data.size() + r_, 0);
  for (size_t i = 0; i < data.size(); ++i) {
    shifted[i] = data[i];
  }

  // Шаг 2: Вычисляем c(x) = m(x) * x^r mod g(x)
  std::vector<uint8_t> remainder = PolynomialMod(shifted);

  // Шаг 3: a(x) = m(x) * x^r + c(x)
  // Добавляем остаток в конец (младшие степени)
  std::vector<uint8_t> codeword(data.size() + r_);
  for (size_t i = 0; i < data.size(); ++i) {
    codeword[i] = data[i];
  }
  for (size_t i = 0; i < remainder.size(); ++i) {
    codeword[data.size() + i] = remainder[i];
  }

  return codeword;
}

std::vector<uint8_t> Crc::ComputeSyndrome(
    const std::vector<uint8_t>& codeword) const {
  if (codeword.size() < static_cast<size_t>(r_)) {
    throw std::invalid_argument("Codeword is too short.");
  }

  for (const auto& bit : codeword) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("Codeword must contain only 0 and 1.");
    }
  }

  return PolynomialMod(codeword);
}

bool Crc::HasError(const std::vector<uint8_t>& codeword) const {
  std::vector<uint8_t> syndrome = ComputeSyndrome(codeword);
  for (const auto& bit : syndrome) {
    if (bit != 0) {
      return true;
    }
  }
  return false;
}

std::vector<uint8_t> Crc::Decode(const std::vector<uint8_t>& codeword) const {
  if (codeword.size() <= static_cast<size_t>(r_)) {
    throw std::invalid_argument("Codeword is too short to decode.");
  }

  // Извлекаем информационную часть (первые k = n - r бит)
  size_t k = codeword.size() - r_;
  std::vector<uint8_t> data(codeword.begin(), codeword.begin() + k);
  return data;
}

std::vector<uint8_t> Crc::PolynomialMod(
    const std::vector<uint8_t>& dividend) const {
  // Копируем делимое для работы
  std::vector<uint8_t> remainder(dividend);

  size_t poly_degree = polynomial_.size() - 1;

  // Полиномиальное деление над GF(2)
  for (size_t i = 0; i + poly_degree < remainder.size(); ++i) {
    if (remainder[i] == 1) {
      // XOR с порождающим многочленом
      for (size_t j = 0; j < polynomial_.size(); ++j) {
        remainder[i + j] ^= polynomial_[j];
      }
    }
  }

  // Возвращаем последние r бит как остаток
  std::vector<uint8_t> result(r_);
  for (int i = 0; i < r_; ++i) {
    result[i] = remainder[remainder.size() - r_ + i];
  }

  return result;
}

}  // namespace harq
