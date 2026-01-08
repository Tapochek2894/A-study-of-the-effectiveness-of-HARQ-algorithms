#include "hamming_encoder.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::vector<uint8_t>> BuildParityCheckMatrix(int r) {
  int n = (1 << r) - 1;
  std::vector<std::vector<uint8_t>> matrix(
      r, std::vector<uint8_t>(n, 0));

  // Столбцы — двоичные индексы 1..n (младший бит в строке 0).
  for (int col = 1; col <= n; col++) {
    for (int row = 0; row < r; row++) {
      matrix[row][col - 1] = static_cast<uint8_t>((col >> row) & 1);
    }
  }

  return matrix;
}

std::vector<uint8_t> ComputeSyndrome(
    const std::vector<std::vector<uint8_t>>& matrix,
    const std::vector<uint8_t>& codeword) {
  std::vector<uint8_t> syndrome(matrix.size(), 0);
  // s = H * c^T над GF(2).
  for (size_t row = 0; row < matrix.size(); row++) {
    uint8_t value = 0;
    for (size_t col = 0; col < codeword.size(); col++) {
      value ^= static_cast<uint8_t>(matrix[row][col] & codeword[col]);
    }
    syndrome[row] = value;
  }
  return syndrome;
}

}  // namespace

TEST(HammingEncoderTest, GeneratorMatrixSize) {
  harq::HammingEncoder encoder(3);
  const auto& matrix = encoder.generator_matrix();

  ASSERT_EQ(matrix.size(), 4u);
  for (const auto& row : matrix) {
    EXPECT_EQ(row.size(), 7u);
  }
}

TEST(HammingEncoderTest, EncodesKnownMessage74) {
  harq::HammingEncoder encoder(3);
  const std::vector<uint8_t> message = {1, 0, 1, 1};
  const std::vector<uint8_t> expected = {0, 1, 1, 0, 0, 1, 1};

  const std::vector<uint8_t> codeword = encoder.Encode(message);

  EXPECT_EQ(codeword, expected);
}

TEST(HammingEncoderTest, EncodedCodewordHasZeroSyndrome1511) {
  harq::HammingEncoder encoder(4);
  const std::vector<uint8_t> message = {1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1};
  const std::vector<uint8_t> codeword = encoder.Encode(message);

  const auto parity_check = BuildParityCheckMatrix(4);
  const std::vector<uint8_t> syndrome =
      ComputeSyndrome(parity_check, codeword);

  EXPECT_EQ(syndrome, std::vector<uint8_t>({0, 0, 0, 0}));
}

TEST(HammingEncoderTest, ThrowsOnInvalidInput) {
  EXPECT_THROW(harq::HammingEncoder(1), std::invalid_argument);

  harq::HammingEncoder encoder(3);
  EXPECT_THROW(encoder.Encode({1, 0}), std::invalid_argument);
  EXPECT_THROW(encoder.Encode({0, 1, 2, 0}), std::invalid_argument);
}

TEST(HammingEncoderTest, EncodesExtendedCodeword74) {
  harq::HammingEncoder encoder(3);
  const std::vector<uint8_t> message = {1, 0, 1, 1};
  const std::vector<uint8_t> codeword = encoder.EncodeExtended(message);

  ASSERT_EQ(codeword.size(), 8u);

  uint8_t parity = 0;
  for (size_t i = 0; i + 1 < codeword.size(); i++) {
    parity ^= codeword[i];
  }
  EXPECT_EQ(codeword.back(), parity);
}
