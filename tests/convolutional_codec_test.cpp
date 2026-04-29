#include "fec/convolutional_codec.hpp"
#include "fec/fec_factory.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

TEST(ConvolutionalCodecTest, ReportsRateOneThirdFrameSizes) {
  harq::fec::FecConfig config;
  config.codec_type = harq::fec::CodecType::kConvolutional;
  config.conv_input_bits_per_frame = 16;
  config.conv_rate_num = 1;
  config.conv_rate_den = 3;
  config.conv_generators = {0133, 0171, 0165};

  auto codec = harq::fec::CreateCodec(config);

  EXPECT_EQ(codec->input_bits_per_frame(), 16);
  EXPECT_EQ(codec->output_bits_per_frame(), (16 + 6) * 3);
}

TEST(ConvolutionalCodecTest, EncodeDecodeSoftRoundTrip) {
  harq::fec::FecConfig config;
  config.codec_type = harq::fec::CodecType::kConvolutional;
  config.conv_input_bits_per_frame = 32;
  config.conv_rate_num = 1;
  config.conv_rate_den = 3;
  config.conv_generators = {0133, 0171, 0165};

  auto codec = harq::fec::CreateCodec(config);

  std::vector<std::uint8_t> info(
      static_cast<std::size_t>(codec->input_bits_per_frame()), 0);
  for (std::size_t i = 0; i < info.size(); ++i) {
    info[i] = static_cast<std::uint8_t>((i * 3 + 1) & 1u);
  }

  const std::vector<std::uint8_t> codeword = codec->Encode(info);
  std::vector<double> soft(codeword.size(), 0.0);
  for (std::size_t i = 0; i < codeword.size(); ++i) {
    soft[i] = codeword[i] ? 4.0 : -4.0;
  }

  EXPECT_EQ(codec->DecodeSoft(soft), info);
}

TEST(ConvolutionalCodecTest, EncodeDecodeHardRoundTrip) {
  harq::fec::FecConfig config;
  config.codec_type = harq::fec::CodecType::kConvolutional;
  config.conv_input_bits_per_frame = 24;
  config.conv_rate_num = 1;
  config.conv_rate_den = 3;
  config.conv_generators = {0133, 0171, 0165};

  auto codec = harq::fec::CreateCodec(config);

  std::vector<std::uint8_t> info(
      static_cast<std::size_t>(codec->input_bits_per_frame()), 0);
  for (std::size_t i = 0; i < info.size(); ++i) {
    info[i] = static_cast<std::uint8_t>((i + (i / 3)) & 1u);
  }

  EXPECT_EQ(codec->DecodeHard(codec->Encode(info)), info);
}
