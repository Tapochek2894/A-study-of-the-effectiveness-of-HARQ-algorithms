#include "awgn_channel.hpp"
#include "bpsk_passband.hpp"
#include "hamming_decoder.hpp"
#include "hamming_encoder.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

std::vector<uint8_t> RoundTripBits(const std::vector<uint8_t> &bits) {
  const int r = 3;
  const harq::HammingEncoder encoder(r);
  const std::vector<uint8_t> codeword = encoder.Encode(bits);

  harq::BpskCarrierConfig config;
  config.carrier_hz = 2.0;
  config.sample_rate_hz = 32.0;
  config.samples_per_symbol = 16;
  config.amplitude = 1.0;
  config.phase = 0.0;

  const std::vector<double> passband =
      harq::BpskPassbandModulate(codeword, config);

  harq::AwgnChannel channel(100.0, 123u);
  const std::vector<double> noisy = channel.AddNoise(passband);
  const std::vector<double> llr = channel.ComputeLlr(noisy);

  const std::vector<uint8_t> received =
      harq::BpskPassbandDemodulate(noisy, config);

  return harq::HammingDecoder(r).Decode(received);
}

} // namespace

TEST(WorkflowTest, BitsRoundTripPassbandWithAwgn) {
  const std::vector<uint8_t> bits = {1, 0, 1, 1};
  const std::vector<uint8_t> decoded = RoundTripBits(bits);

  EXPECT_EQ(decoded, bits);
}
