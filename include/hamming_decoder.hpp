#pragma once

#include <cstdint>
#include <vector>

namespace harq {

// Модуль декодера Хэмминга: исправляет одиночную ошибку по синдрому.
class HammingDecoder {
 public:
  explicit HammingDecoder(int r);

  int n() const;
  int k() const;

  // Возвращает исправленное кодовое слово (или исходное, если ошибок нет).
  std::vector<uint8_t> Correct(const std::vector<uint8_t>& codeword) const;

  // Исправляет кодовое слово и извлекает k информационных битов.
  std::vector<uint8_t> Decode(const std::vector<uint8_t>& codeword) const;

 private:
  static bool IsPowerOfTwo(int value);
  int ComputeSyndrome(const std::vector<uint8_t>& codeword) const;

  int r_;
  int n_;
  int k_;
  std::vector<int> data_positions_;
};

}  // namespace harq
