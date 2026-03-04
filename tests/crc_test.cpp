#include "crc.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

// Тесты основаны на примерах из документа Буркова (раздел 1.2).
// Пример: g(x) = x³ + x + 1, m = 1010
// c(x) = (x³ + x) * x³ mod (x³ + x + 1) = x + 1 → 011
// a = 1010011

TEST(CrcTest, ConstructorValidPolynomial) {
  // x³ + x + 1 = [1, 0, 1, 1]
  std::vector<uint8_t> poly = {1, 0, 1, 1};
  harq::Crc crc(poly);

  EXPECT_EQ(crc.r(), 3);
  EXPECT_EQ(crc.polynomial(), poly);
}

TEST(CrcTest, ConstructorThrowsOnInvalidPolynomial) {
  // Слишком короткий многочлен
  EXPECT_THROW(harq::Crc({1}), std::invalid_argument);

  // Старший коэффициент не 1
  EXPECT_THROW(harq::Crc({0, 1, 1}), std::invalid_argument);

  // Недопустимые значения
  EXPECT_THROW(harq::Crc({1, 2, 1}), std::invalid_argument);
}

TEST(CrcTest, EncodeExampleFromBurkov) {
  // Пример из документа Буркова (раздел 1.2.1):
  // g(x) = x³ + x + 1, m = 1010, ожидается a = 1010011
  std::vector<uint8_t> poly = {1, 0, 1, 1};  // x³ + x + 1
  harq::Crc crc(poly);

  std::vector<uint8_t> message = {1, 0, 1, 0};
  std::vector<uint8_t> expected = {1, 0, 1, 0, 0, 1, 1};

  std::vector<uint8_t> codeword = crc.Encode(message);

  EXPECT_EQ(codeword, expected);
}

TEST(CrcTest, EncodeOutputLength) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};  // r = 3
  harq::Crc crc(poly);

  std::vector<uint8_t> message = {1, 1, 0, 1, 0, 1};  // k = 6
  std::vector<uint8_t> codeword = crc.Encode(message);

  // n = k + r = 6 + 3 = 9
  EXPECT_EQ(codeword.size(), 9u);
}

TEST(CrcTest, EncodedCodewordHasZeroSyndrome) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};
  harq::Crc crc(poly);

  std::vector<uint8_t> message = {1, 0, 1, 0};
  std::vector<uint8_t> codeword = crc.Encode(message);

  std::vector<uint8_t> syndrome = crc.ComputeSyndrome(codeword);

  EXPECT_EQ(syndrome, std::vector<uint8_t>({0, 0, 0}));
  EXPECT_FALSE(crc.HasError(codeword));
}

TEST(CrcTest, CorruptedCodewordHasNonzeroSyndrome) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};
  harq::Crc crc(poly);

  std::vector<uint8_t> message = {1, 0, 1, 0};
  std::vector<uint8_t> codeword = crc.Encode(message);

  // Вносим ошибку в один бит
  codeword[2] ^= 1;

  EXPECT_TRUE(crc.HasError(codeword));
}

TEST(CrcTest, DecodeExtractsOriginalMessage) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};
  harq::Crc crc(poly);

  std::vector<uint8_t> message = {1, 0, 1, 0};
  std::vector<uint8_t> codeword = crc.Encode(message);
  std::vector<uint8_t> decoded = crc.Decode(codeword);

  EXPECT_EQ(decoded, message);
}

TEST(CrcTest, EncodeThrowsOnEmptyInput) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};
  harq::Crc crc(poly);

  EXPECT_THROW(crc.Encode({}), std::invalid_argument);
}

TEST(CrcTest, EncodeThrowsOnInvalidInput) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};
  harq::Crc crc(poly);

  EXPECT_THROW(crc.Encode({1, 2, 0}), std::invalid_argument);
}

TEST(CrcTest, ComputeSyndromeThrowsOnShortCodeword) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};  // r = 3
  harq::Crc crc(poly);

  EXPECT_THROW(crc.ComputeSyndrome({1, 0}), std::invalid_argument);
}

TEST(CrcTest, Crc4Polynomial) {
  // x⁴ + x³ + x² + 1 из таблицы 2.1 Буркова
  std::vector<uint8_t> poly = {1, 1, 1, 0, 1};
  harq::Crc crc(poly);

  EXPECT_EQ(crc.r(), 4);

  std::vector<uint8_t> message = {1, 0, 1};
  std::vector<uint8_t> codeword = crc.Encode(message);

  EXPECT_EQ(codeword.size(), 7u);  // 3 + 4
  EXPECT_FALSE(crc.HasError(codeword));

  // Проверяем, что декодирование возвращает исходное сообщение
  EXPECT_EQ(crc.Decode(codeword), message);
}

TEST(CrcTest, MultipleBitErrors) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};
  harq::Crc crc(poly);

  std::vector<uint8_t> message = {1, 1, 0, 0, 1, 0, 1, 1};
  std::vector<uint8_t> codeword = crc.Encode(message);

  // Вносим несколько ошибок
  codeword[0] ^= 1;
  codeword[3] ^= 1;
  codeword[5] ^= 1;

  EXPECT_TRUE(crc.HasError(codeword));
}

TEST(CrcTest, AllZerosMessage) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};
  harq::Crc crc(poly);

  std::vector<uint8_t> message = {0, 0, 0, 0};
  std::vector<uint8_t> codeword = crc.Encode(message);

  // Для нулевого сообщения CRC тоже должен быть нулевым
  EXPECT_EQ(codeword, std::vector<uint8_t>({0, 0, 0, 0, 0, 0, 0}));
  EXPECT_FALSE(crc.HasError(codeword));
}

TEST(CrcTest, AllOnesMessage) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};
  harq::Crc crc(poly);

  std::vector<uint8_t> message = {1, 1, 1, 1};
  std::vector<uint8_t> codeword = crc.Encode(message);

  EXPECT_EQ(codeword.size(), 7u);
  EXPECT_FALSE(crc.HasError(codeword));
  EXPECT_EQ(crc.Decode(codeword), message);
}

TEST(CrcTest, Crc8Polynomial) {
  // x⁸ + x⁷ + x⁶ + x⁴ + x² + 1 из таблицы 2.1 Буркова
  std::vector<uint8_t> poly = {1, 1, 1, 0, 1, 0, 1, 0, 1};
  harq::Crc crc(poly);

  EXPECT_EQ(crc.r(), 8);

  std::vector<uint8_t> message = {1, 0, 1, 1};
  std::vector<uint8_t> codeword = crc.Encode(message);

  EXPECT_EQ(codeword.size(), 12u);  // 4 + 8
  EXPECT_FALSE(crc.HasError(codeword));
}

TEST(CrcTest, DecodeThrowsOnShortCodeword) {
  std::vector<uint8_t> poly = {1, 0, 1, 1};  // r = 3
  harq::Crc crc(poly);

  // Кодовое слово длиной r или меньше не может быть декодировано
  EXPECT_THROW(crc.Decode({1, 0, 1}), std::invalid_argument);
  EXPECT_THROW(crc.Decode({1, 0}), std::invalid_argument);
}
