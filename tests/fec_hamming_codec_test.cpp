#include "fec/fec_factory.hpp"

#include <gtest/gtest.h>

#include <vector>

TEST(FecHammingCodecTest, ReportsFrameSizes) {
  harq::fec::FecConfig config;
  config.codec_type = harq::fec::CodecType::kHamming;
  config.hamming_r = 3;

  auto codec = harq::fec::CreateCodec(config);

  EXPECT_EQ(codec->input_bits_per_frame(), 4);
  EXPECT_EQ(codec->output_bits_per_frame(), 7);
}

TEST(FecHammingCodecTest, EncodeDecodeHardRoundTrip) {
  harq::fec::FecConfig config;
  config.codec_type = harq::fec::CodecType::kHamming;
  config.hamming_r = 3;

  auto codec = harq::fec::CreateCodec(config);

  const std::vector<uint8_t> info = {1, 0, 1, 1};
  std::vector<uint8_t> codeword = codec->Encode(info);
  codeword[2] ^= 1;

  const std::vector<uint8_t> decoded = codec->DecodeHard(codeword);
  EXPECT_EQ(decoded, info);
}

TEST(FecHammingCodecTest, EncodeDecodeSoftRoundTrip) {
  harq::fec::FecConfig config;
  config.codec_type = harq::fec::CodecType::kHamming;
  config.hamming_r = 3;

  auto codec = harq::fec::CreateCodec(config);

  const std::vector<uint8_t> info = {1, 0, 1, 1};
  const std::vector<uint8_t> codeword = codec->Encode(info);

  std::vector<double> soft_bits;
  soft_bits.reserve(codeword.size());
  for (uint8_t bit : codeword) {
    soft_bits.push_back(bit == 1 ? 2.0 : -2.0);
  }

  const std::vector<uint8_t> decoded = codec->DecodeSoft(soft_bits);
  EXPECT_EQ(decoded, info);
}

TEST(FecFactoryTest, ConvolutionalCodecUnavailableWithoutAff3ct) {
  harq::fec::FecConfig config;
  config.codec_type = harq::fec::CodecType::kConvolutionalAff3ct;

  auto codec = harq::fec::CreateCodec(config);

  std::vector<uint8_t> info(
      static_cast<std::size_t>(codec->input_bits_per_frame()), 0);
  EXPECT_THROW(codec->Encode(info), std::invalid_argument);
}

TEST(FecFactoryTest, ConvolutionalCodecReportsConfiguredFrameSizes) {
  harq::fec::FecConfig config;
  config.codec_type = harq::fec::CodecType::kConvolutionalAff3ct;
  config.conv_input_bits_per_frame = 256;

  auto codec = harq::fec::CreateCodec(config);

  EXPECT_EQ(codec->input_bits_per_frame(), 256);
  EXPECT_EQ(codec->output_bits_per_frame(), 512);
}

TEST(FecFactoryTest, ConvolutionalCodecRejectsInvalidFrameSize) {
  harq::fec::FecConfig config;
  config.codec_type = harq::fec::CodecType::kConvolutionalAff3ct;
  config.conv_input_bits_per_frame = 0;

  EXPECT_THROW(harq::fec::CreateCodec(config), std::invalid_argument);
}
