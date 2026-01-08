#include "hamming_decoder.hpp"
#include "hamming_encoder.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

TEST(HammingDecoderTest, CorrectsSingleError74) {
  harq::HammingEncoder encoder(3);
  harq::HammingDecoder decoder(3);

  const std::vector<uint8_t> message = {1, 0, 1, 1};
  std::vector<uint8_t> codeword = encoder.Encode(message);
  codeword[2] ^= 1;

  const std::vector<uint8_t> decoded = decoder.Decode(codeword);

  EXPECT_EQ(decoded, message);
}

TEST(HammingDecoderTest, DecodesWithoutErrors1511) {
  harq::HammingEncoder encoder(4);
  harq::HammingDecoder decoder(4);

  const std::vector<uint8_t> message = {1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1};
  const std::vector<uint8_t> codeword = encoder.Encode(message);

  const std::vector<uint8_t> decoded = decoder.Decode(codeword);

  EXPECT_EQ(decoded, message);
}

TEST(HammingDecoderTest, CorrectsSingleError1511) {
  harq::HammingEncoder encoder(4);
  harq::HammingDecoder decoder(4);

  const std::vector<uint8_t> message = {1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1};
  std::vector<uint8_t> codeword = encoder.Encode(message);
  codeword[10] ^= 1;

  const std::vector<uint8_t> decoded = decoder.Decode(codeword);

  EXPECT_EQ(decoded, message);
}

TEST(HammingDecoderTest, ThrowsOnInvalidInput) {
  EXPECT_THROW(harq::HammingDecoder(1), std::invalid_argument);

  harq::HammingDecoder decoder(3);
  EXPECT_THROW(decoder.Decode({1, 0}), std::invalid_argument);
  EXPECT_THROW(decoder.Decode({0, 1, 2, 0, 1, 0, 1}), std::invalid_argument);
}

TEST(HammingDecoderTest, SecdedCorrectsSingleError74) {
  harq::HammingEncoder encoder(3);
  harq::HammingDecoder decoder(3);

  const std::vector<uint8_t> message = {1, 0, 1, 1};
  std::vector<uint8_t> codeword = encoder.Encode(message);

  uint8_t parity = 0;
  for (uint8_t bit : codeword) {
    parity ^= bit;
  }
  codeword.push_back(parity);

  codeword[4] ^= 1;

  auto result = decoder.DecodeWithStatus(codeword);
  EXPECT_EQ(result.first, message);
  EXPECT_EQ(result.second, harq::HammingDecoder::DecodeStatus::kCorrected);
}

TEST(HammingDecoderTest, SecdedDetectsDoubleError74) {
  harq::HammingEncoder encoder(3);
  harq::HammingDecoder decoder(3);

  const std::vector<uint8_t> message = {1, 0, 1, 1};
  std::vector<uint8_t> codeword = encoder.Encode(message);

  uint8_t parity = 0;
  for (uint8_t bit : codeword) {
    parity ^= bit;
  }
  codeword.push_back(parity);

  codeword[1] ^= 1;
  codeword[5] ^= 1;

  auto result = decoder.DecodeWithStatus(codeword);
  EXPECT_EQ(result.second, harq::HammingDecoder::DecodeStatus::kDetectedDouble);
}

TEST(HammingDecoderTest, SecdedCorrectsParityBit74) {
  harq::HammingEncoder encoder(3);
  harq::HammingDecoder decoder(3);

  const std::vector<uint8_t> message = {1, 0, 1, 1};
  std::vector<uint8_t> codeword = encoder.Encode(message);

  uint8_t parity = 0;
  for (uint8_t bit : codeword) {
    parity ^= bit;
  }
  codeword.push_back(parity);

  codeword.back() ^= 1;

  auto result = decoder.DecodeWithStatus(codeword);
  EXPECT_EQ(result.first, message);
  EXPECT_EQ(result.second,
            harq::HammingDecoder::DecodeStatus::kParityCorrected);
}
