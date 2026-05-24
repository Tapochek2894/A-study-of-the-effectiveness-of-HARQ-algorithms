#include "fec/convolutional_cc_codec.hpp"
#include "fec/fec_config.hpp"
#include "fec/fec_factory.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

namespace {

harq::fec::FecConfig MakeConfig(int k) {
  harq::fec::FecConfig cfg{};
  cfg.codec_type = harq::fec::CodecType::kConvolutionalCc;
  cfg.cc_input_bits_per_frame = k;
  cfg.cc_constraint_length = 7;
  cfg.cc_generators_octal = {0133u, 0171u, 0165u};  // rate 1/3, d_free = 15
  return cfg;
}

std::vector<uint8_t> RandomBits(int k, uint32_t seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> bit(0, 1);
  std::vector<uint8_t> v(static_cast<std::size_t>(k));
  for (auto& b : v) b = static_cast<uint8_t>(bit(rng));
  return v;
}

}  // namespace

TEST(ConvolutionalCcCodecTest, FrameSizesMatchRateOneThird) {
  harq::fec::ConvolutionalCcCodec codec(MakeConfig(512));
  EXPECT_EQ(codec.input_bits_per_frame(), 512);
  // output = n * (k + K-1) = 3 * (512 + 6) = 1554.
  EXPECT_EQ(codec.output_bits_per_frame(), 3 * (512 + 6));
  EXPECT_EQ(codec.rate_den(), 3);
  EXPECT_EQ(codec.constraint_length(), 7);
}

TEST(ConvolutionalCcCodecTest, FactoryBuildsCcCodec) {
  auto codec = harq::fec::CreateCodec(MakeConfig(64));
  EXPECT_EQ(codec->input_bits_per_frame(), 64);
  EXPECT_EQ(codec->output_bits_per_frame(), 3 * (64 + 6));
}

TEST(ConvolutionalCcCodecTest, NoiselessRoundTripHard) {
  const int k = 200;
  harq::fec::ConvolutionalCcCodec codec(MakeConfig(k));
  const std::vector<uint8_t> info = RandomBits(k, 1234u);
  const std::vector<uint8_t> code = codec.Encode(info);
  EXPECT_EQ(codec.DecodeHard(code), info);
}

TEST(ConvolutionalCcCodecTest, NoiselessRoundTripSoft) {
  const int k = 200;
  harq::fec::ConvolutionalCcCodec codec(MakeConfig(k));
  const std::vector<uint8_t> info = RandomBits(k, 7u);
  const std::vector<uint8_t> code = codec.Encode(info);
  // Сильные LLR с правильным знаком: бит 1 → +L, бит 0 → −L.
  std::vector<double> soft(code.size());
  for (std::size_t i = 0; i < code.size(); ++i) {
    soft[i] = code[i] ? 8.0 : -8.0;
  }
  EXPECT_EQ(codec.DecodeSoft(soft), info);
}

TEST(ConvolutionalCcCodecTest, CorrectsSparseBitErrors) {
  const int k = 100;
  harq::fec::ConvolutionalCcCodec codec(MakeConfig(k));
  const std::vector<uint8_t> info = RandomBits(k, 99u);
  std::vector<uint8_t> code = codec.Encode(info);
  // Несколько разнесённых ошибок — d_free = 15 уверенно исправляет.
  for (std::size_t pos : {std::size_t{3}, std::size_t{50}, std::size_t{120},
                          std::size_t{200}}) {
    if (pos < code.size()) code[pos] ^= 1u;
  }
  EXPECT_EQ(codec.DecodeHard(code), info);
}

TEST(ConvolutionalCcCodecTest, RejectsBadGenerators) {
  harq::fec::FecConfig cfg = MakeConfig(16);
  cfg.cc_generators_octal = {0133u};  // только один генератор — rate >= 1
  EXPECT_THROW(harq::fec::ConvolutionalCcCodec{cfg}, std::invalid_argument);

  cfg = MakeConfig(16);
  cfg.cc_generators_octal = {0133u, 0u};  // нулевой генератор
  EXPECT_THROW(harq::fec::ConvolutionalCcCodec{cfg}, std::invalid_argument);
}
