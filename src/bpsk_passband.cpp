#include "bpsk_passband.hpp"

#include <cmath>
#include <stdexcept>

namespace harq {

namespace {

constexpr double kTwoPi = 6.2831853071795864769;

void ValidateCarrierConfig(const BpskCarrierConfig& config) {
  if (!std::isfinite(config.carrier_hz) ||
      !std::isfinite(config.sample_rate_hz) ||
      !std::isfinite(config.amplitude) || !std::isfinite(config.phase)) {
    throw std::invalid_argument("BPSK carrier config must be finite.");
  }
  if (config.sample_rate_hz <= 0.0) {
    throw std::invalid_argument("Sample rate must be positive.");
  }
  if (config.samples_per_symbol <= 0) {
    throw std::invalid_argument("Samples per symbol must be positive.");
  }
  if (config.carrier_hz < 0.0) {
    throw std::invalid_argument("Carrier frequency must be non-negative.");
  }
}

double CarrierAt(const BpskCarrierConfig& config, std::size_t sample_index) {
  const double t =
      static_cast<double>(sample_index) / config.sample_rate_hz;
  return std::cos(kTwoPi * config.carrier_hz * t + config.phase);
}

}  // namespace

BpskPassbandModulator::BpskPassbandModulator(BpskCarrierConfig config)
    : config_(config) {
  ValidateCarrierConfig(config_);
}

std::vector<double> BpskPassbandModulator::Modulate(
    const std::vector<uint8_t>& bits) const {
  ValidateCarrierConfig(config_);

  std::vector<double> samples;
  samples.reserve(bits.size() * config_.samples_per_symbol);

  std::size_t sample_index = 0;
  for (uint8_t bit : bits) {
    const double symbol = (bit == 0) ? -1.0 : (bit == 1 ? 1.0 : 0.0);
    if (symbol == 0.0) {
      throw std::invalid_argument("BPSK passband modulator expects bits 0 or 1.");
    }
    for (int k = 0; k < config_.samples_per_symbol; ++k) {
      const double carrier = CarrierAt(config_, sample_index++);
      samples.push_back(config_.amplitude * symbol * carrier);
    }
  }

  return samples;
}

BpskPassbandDemodulator::BpskPassbandDemodulator(BpskCarrierConfig config)
    : config_(config) {
  ValidateCarrierConfig(config_);
}

std::vector<uint8_t> BpskPassbandDemodulator::Demodulate(
    const std::vector<double>& samples) const {
  ValidateCarrierConfig(config_);

  if (samples.size() %
          static_cast<std::size_t>(config_.samples_per_symbol) !=
      0) {
    throw std::invalid_argument(
        "Sample count must be a multiple of samples per symbol.");
  }

  const std::size_t symbols_count =
      samples.size() / static_cast<std::size_t>(config_.samples_per_symbol);
  std::vector<uint8_t> bits;
  bits.reserve(symbols_count);

  std::size_t sample_index = 0;
  for (std::size_t symbol_index = 0; symbol_index < symbols_count;
       ++symbol_index) {
    double accum = 0.0;
    for (int k = 0; k < config_.samples_per_symbol; ++k) {
      const double carrier = CarrierAt(config_, sample_index);
      accum += samples[sample_index] * carrier;
      ++sample_index;
    }
    bits.push_back(accum >= 0.0 ? 1 : 0);
  }

  return bits;
}

std::vector<double> BpskPassbandModulate(
    const std::vector<uint8_t>& bits,
    BpskCarrierConfig config) {
  return BpskPassbandModulator(config).Modulate(bits);
}

std::vector<uint8_t> BpskPassbandDemodulate(
    const std::vector<double>& samples,
    BpskCarrierConfig config) {
  return BpskPassbandDemodulator(config).Demodulate(samples);
}

}  // namespace harq
