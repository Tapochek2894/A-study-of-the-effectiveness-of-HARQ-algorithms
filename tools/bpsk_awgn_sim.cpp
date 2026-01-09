#include "awgn_channel.hpp"
#include "bpsk.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
  std::size_t bits = 200000;
  uint32_t seed = 5489u;
  std::vector<double> snr_list = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  bool use_range = false;
  double snr_start = 0.0;
  double snr_end = 10.0;
  int snr_points = 11;
};

void PrintUsage(const char* argv0) {
  std::cout << "Usage: " << argv0
            << " [--bits <count>] [--seed <seed>]"
            << " [--snr <dB1,dB2,...>]"
            << " [--snr-start <dB> --snr-end <dB> --snr-points <n>]\n";
}

bool ParseSnrList(const std::string& value, std::vector<double>* out) {
  std::vector<double> parsed;
  std::stringstream ss(value);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) {
      return false;
    }
    char* endptr = nullptr;
    double v = std::strtod(token.c_str(), &endptr);
    if (endptr == token.c_str()) {
      return false;
    }
    parsed.push_back(v);
  }
  if (parsed.empty()) {
    return false;
  }
  *out = std::move(parsed);
  return true;
}

bool ParseArgs(int argc, char** argv, Options* options) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--bits" && i + 1 < argc) {
      options->bits = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      options->seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--snr" && i + 1 < argc) {
      if (!ParseSnrList(argv[++i], &options->snr_list)) {
        return false;
      }
      options->use_range = false;
    } else if (arg == "--snr-start" && i + 1 < argc) {
      options->snr_start = std::stod(argv[++i]);
      options->use_range = true;
    } else if (arg == "--snr-end" && i + 1 < argc) {
      options->snr_end = std::stod(argv[++i]);
      options->use_range = true;
    } else if (arg == "--snr-points" && i + 1 < argc) {
      options->snr_points = std::stoi(argv[++i]);
      options->use_range = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!ParseArgs(argc, argv, &options)) {
    PrintUsage(argv[0]);
    return 1;
  }
  if (options.bits == 0) {
    std::cerr << "Bits must be positive.\n";
    return 1;
  }
  if (options.use_range) {
    if (options.snr_points < 2) {
      std::cerr << "snr-points must be at least 2.\n";
      return 1;
    }
    if (!(options.snr_end > options.snr_start)) {
      std::cerr << "Invalid SNR range.\n";
      return 1;
    }
  }

  std::vector<double> snr_values = options.snr_list;
  if (options.use_range) {
    snr_values.clear();
    for (int i = 0; i < options.snr_points; ++i) {
      double t = static_cast<double>(i) /
          static_cast<double>(options.snr_points - 1);
      double value =
          options.snr_start + t * (options.snr_end - options.snr_start);
      snr_values.push_back(value);
    }
  }

  std::mt19937 rng(options.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);

  std::cout << "snr_db,ber,bit_errors,total_bits\n";
  std::cout << std::setprecision(8) << std::fixed;

  harq::BpskModulator modulator;
  harq::BpskDemodulator demodulator;

  for (std::size_t idx = 0; idx < snr_values.size(); ++idx) {
    double snr_db = snr_values[idx];
    if (!std::isfinite(snr_db)) {
      std::cerr << "Invalid SNR value: " << snr_db << "\n";
      return 1;
    }
    harq::AwgnChannel channel(snr_db,
                              static_cast<uint32_t>(options.seed + idx));

    std::vector<uint8_t> bits(options.bits, 0);
    for (std::size_t i = 0; i < options.bits; ++i) {
      bits[i] = static_cast<uint8_t>(bit_dist(rng));
    }

    std::vector<double> symbols = modulator.Modulate(bits);
    std::vector<double> received = channel.AddNoise(symbols);
    std::vector<uint8_t> decoded = demodulator.Demodulate(received);

    std::size_t bit_errors = 0;
    for (std::size_t i = 0; i < options.bits; ++i) {
      if (decoded[i] != bits[i]) {
        ++bit_errors;
      }
    }

    double ber = static_cast<double>(bit_errors) /
        static_cast<double>(options.bits);
    std::cout << snr_db << "," << ber << "," << bit_errors << ","
              << options.bits << "\n";
  }

  return 0;
}
