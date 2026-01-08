#pragma once

#include <cstdint>
#include <vector>

namespace harq {

// Модуль кодера Хэмминга: строит коды (2^r - 1, 2^r - 1 - r) и матрицу G.
// Позиции паритетов находятся на степенях двойки, нумерация битов начинается с 1.
class HammingEncoder {
 public:
  explicit HammingEncoder(int r);

  int n() const;
  int k() const;

  const std::vector<std::vector<uint8_t>>& generator_matrix() const;

  // Кодирует k битов данных в n-битовое кодовое слово Хэмминга через матрицу G.
  std::vector<uint8_t> Encode(const std::vector<uint8_t>& data) const;

  // Кодирует k битов данных в расширенное кодовое слово (n+1) с общим паритетом.
  std::vector<uint8_t> EncodeExtended(const std::vector<uint8_t>& data) const;

 private:
  static bool IsPowerOfTwo(int value);
  std::vector<uint8_t> BuildCodewordFromData(
      const std::vector<uint8_t>& data) const;

  int r_;
  int n_;
  int k_;
  std::vector<int> parity_positions_;
  std::vector<int> data_positions_;
  std::vector<std::vector<uint8_t>> generator_;
};

}  // namespace harq
