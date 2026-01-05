#include "bpsk.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

TEST(BpskModulatorTest, ModulatesBitsToSymbols) {
  const std::vector<uint8_t> bits = {0, 1, 1, 0};
  const std::vector<double> expected = {-1.0, 1.0, 1.0, -1.0};

  harq::BpskModulator modulator;
  const std::vector<double> symbols = modulator.Modulate(bits);

  ASSERT_EQ(symbols, expected);
}

TEST(BpskModulatorTest, ThrowsOnInvalidBit) {
  const std::vector<uint8_t> bits = {0, 2, 1};

  harq::BpskModulator modulator;
  EXPECT_THROW(modulator.Modulate(bits), std::invalid_argument);
}
