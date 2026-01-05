#include "bpsk_passband.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

TEST(BpskPassbandTest, ModulateAndDemodulateRoundTrip) {
  const std::vector<uint8_t> bits = {1, 0, 1, 1, 0};

  harq::BpskCarrierConfig config;
  config.carrier_hz = 2.0;
  config.sample_rate_hz = 32.0;
  config.samples_per_symbol = 16;
  config.amplitude = 1.0;
  config.phase = 0.0;

  const std::vector<double> samples = harq::BpskPassbandModulate(bits, config);
  const std::vector<uint8_t> recovered =
      harq::BpskPassbandDemodulate(samples, config);

  ASSERT_EQ(recovered, bits);
}
