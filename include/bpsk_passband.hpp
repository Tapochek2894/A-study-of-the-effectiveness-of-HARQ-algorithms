#pragma once

#include <cstdint>
#include <vector>

namespace harq {

struct BpskCarrierConfig {
  double carrier_hz = 0.0;
  double sample_rate_hz = 1.0;
  int samples_per_symbol = 1;
  double amplitude = 1.0;
  double phase = 0.0;
};

class BpskPassbandModulator {
 public:
  explicit BpskPassbandModulator(BpskCarrierConfig config);

  std::vector<double> Modulate(const std::vector<uint8_t>& bits) const;

 private:
  BpskCarrierConfig config_;
};

class BpskPassbandDemodulator {
 public:
  explicit BpskPassbandDemodulator(BpskCarrierConfig config);

  std::vector<uint8_t> Demodulate(
      const std::vector<double>& samples) const;

 private:
  BpskCarrierConfig config_;
};

std::vector<double> BpskPassbandModulate(
    const std::vector<uint8_t>& bits,
    BpskCarrierConfig config);

std::vector<uint8_t> BpskPassbandDemodulate(
    const std::vector<double>& samples,
    BpskCarrierConfig config);

}  // namespace harq
