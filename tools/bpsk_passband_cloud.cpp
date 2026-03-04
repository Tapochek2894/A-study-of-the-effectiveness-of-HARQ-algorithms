#include "awgn_channel.hpp"
#include "bpsk_passband.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kTwoPi = 6.2831853071795864769;

double CarrierCos(const harq::BpskCarrierConfig& config,
                  std::size_t sample_index) {
  const double t =
      static_cast<double>(sample_index) / config.sample_rate_hz;
  return std::cos(kTwoPi * config.carrier_hz * t + config.phase);
}

double CarrierSin(const harq::BpskCarrierConfig& config,
                  std::size_t sample_index) {
  const double t =
      static_cast<double>(sample_index) / config.sample_rate_hz;
  return std::sin(kTwoPi * config.carrier_hz * t + config.phase);
}

void AddNoise(double snr_db,
              double signal_power,
              std::vector<double>& samples,
              uint32_t seed) {
  if (!std::isfinite(signal_power) || signal_power <= 0.0) {
    throw std::invalid_argument("Signal power must be positive and finite.");
  }

  const double snr_db_for_unity = snr_db - 10.0 * std::log10(signal_power);
  harq::AwgnChannel channel(snr_db_for_unity, seed);
  samples = channel.AddNoise(samples);
}

void PrintUsage(const char* name) {
  std::cerr
      << "Usage: " << name
      << " <output_csv> [num_symbols] [snr_db] [carrier_hz] [sample_rate_hz]"
         " [samples_per_symbol] [amplitude] [phase_rad] [seed]\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 1;
  }

  const std::string output_path = argv[1];
  std::size_t num_symbols = (argc > 2) ? std::stoul(argv[2]) : 20000;
  const double snr_db = (argc > 3) ? std::stod(argv[3]) : 6.0;
  const double carrier_hz = (argc > 4) ? std::stod(argv[4]) : 1000.0;
  const double sample_rate_hz = (argc > 5) ? std::stod(argv[5]) : 10000.0;
  const int samples_per_symbol = (argc > 6) ? std::stoi(argv[6]) : 4;
  const double amplitude = (argc > 7) ? std::stod(argv[7]) : 1.0;
  const double phase = (argc > 8) ? std::stod(argv[8]) : 0.0;
  const uint32_t seed = (argc > 9) ? static_cast<uint32_t>(std::stoul(argv[9]))
                                   : 5489u;

  if (num_symbols == 0) {
    throw std::invalid_argument("Number of symbols must be positive.");
  }
  if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0) {
    throw std::invalid_argument("Sample rate must be positive and finite.");
  }
  if (samples_per_symbol <= 0) {
    throw std::invalid_argument("Samples per symbol must be positive.");
  }
  if (!std::isfinite(amplitude) || amplitude <= 0.0) {
    throw std::invalid_argument("Amplitude must be positive and finite.");
  }
  if (!std::isfinite(carrier_hz) || carrier_hz < 0.0) {
    throw std::invalid_argument("Carrier frequency must be non-negative.");
  }

  harq::BpskCarrierConfig config;
  config.carrier_hz = carrier_hz;
  config.sample_rate_hz = sample_rate_hz;
  config.samples_per_symbol = samples_per_symbol;
  config.amplitude = amplitude;
  config.phase = phase;

  std::mt19937 bit_rng(seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);
  std::vector<uint8_t> bits;
  bits.reserve(num_symbols);
  for (std::size_t i = 0; i < num_symbols; ++i) {
    bits.push_back(static_cast<uint8_t>(bit_dist(bit_rng)));
  }

  harq::BpskPassbandModulator modulator(config);
  std::vector<double> samples = modulator.Modulate(bits);

  const double signal_power = (amplitude * amplitude) * 0.5;
  AddNoise(snr_db, signal_power, samples, seed + 1u);

  std::ofstream out(output_path);
  if (!out) {
    throw std::runtime_error("Failed to open output file.");
  }

  out << "bit,i,q\n";
  out << std::fixed << std::setprecision(8);

  const double scale = 2.0 / static_cast<double>(samples_per_symbol);
  std::size_t sample_index = 0;
  for (std::size_t symbol_index = 0; symbol_index < num_symbols;
       ++symbol_index) {
    double i_acc = 0.0;
    double q_acc = 0.0;
    for (int k = 0; k < samples_per_symbol; ++k) {
      const double cos_val = CarrierCos(config, sample_index);
      const double sin_val = CarrierSin(config, sample_index);
      i_acc += samples[sample_index] * cos_val;
      q_acc -= samples[sample_index] * sin_val;
      ++sample_index;
    }
    const double i_val = i_acc * scale;
    const double q_val = q_acc * scale;
    out << static_cast<int>(bits[symbol_index]) << "," << i_val << "," << q_val
        << "\n";
  }

  std::cout << "Saved " << num_symbols << " symbols to " << output_path << "\n";
  return 0;
}
