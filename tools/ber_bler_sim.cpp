#include "hamming_decoder.hpp"
#include "hamming_encoder.hpp"

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
  int r = 3;
  std::size_t blocks = 10000;
  uint32_t seed = 5489u;
  std::vector<double> p_list = {1e-4, 5e-4, 1e-3, 5e-3, 1e-2};
  bool use_range = false;
  double p_start = 1e-4;
  double p_end = 1e-2;
  int p_points = 9;
  bool p_log = true;
};

struct ErrorOutcome {
  uint8_t weight = 0;
  uint8_t info_errors = 0;
  bool block_error = false;
};

std::vector<ErrorOutcome> PrecomputeErrorOutcomes(int r) {
  harq::HammingDecoder decoder(r);
  const int n = decoder.n();
  const int k = decoder.k();
  if (n <= 0 || n > 30) {
    throw std::invalid_argument("Unsupported n for exhaustive enumeration.");
  }

  std::vector<ErrorOutcome> outcomes;
  const std::uint32_t total = 1u << n;
  outcomes.reserve(total);

  std::vector<uint8_t> error_vec(static_cast<std::size_t>(n), 0);
  for (std::uint32_t mask = 0; mask < total; ++mask) {
    for (int bit = 0; bit < n; ++bit) {
      error_vec[static_cast<std::size_t>(bit)] =
          static_cast<uint8_t>((mask >> bit) & 1u);
    }

    std::vector<uint8_t> decoded = decoder.Decode(error_vec);
    int info_errors = 0;
    for (uint8_t bit : decoded) {
      info_errors += (bit == 1) ? 1 : 0;
    }

    ErrorOutcome outcome;
    outcome.weight = static_cast<uint8_t>(__builtin_popcount(mask));
    outcome.info_errors = static_cast<uint8_t>(info_errors);
    outcome.block_error = info_errors > 0;
    outcomes.push_back(outcome);
  }

  if (static_cast<int>(outcomes.size()) != (1 << n)) {
    throw std::runtime_error("Error outcome enumeration failed.");
  }

  return outcomes;
}

void PrintUsage(const char* argv0) {
  std::cout << "Usage: " << argv0
            << " [--r <parity_bits>] [--blocks <count>] [--seed <seed>]"
            << " [--p <p1,p2,...>]"
            << " [--p-start <value> --p-end <value> --p-points <n> [--p-linear]]\n";
}

bool ParsePList(const std::string& value, std::vector<double>* out) {
  std::vector<double> parsed;
  std::stringstream ss(value);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) {
      return false;
    }
    char* endptr = nullptr;
    double p = std::strtod(token.c_str(), &endptr);
    if (endptr == token.c_str() || p < 0.0 || p > 1.0) {
      return false;
    }
    parsed.push_back(p);
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
    if (arg == "--r" && i + 1 < argc) {
      options->r = std::stoi(argv[++i]);
    } else if (arg == "--blocks" && i + 1 < argc) {
      options->blocks = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      options->seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--p" && i + 1 < argc) {
      if (!ParsePList(argv[++i], &options->p_list)) {
        return false;
      }
      options->use_range = false;
    } else if (arg == "--p-start" && i + 1 < argc) {
      options->p_start = std::stod(argv[++i]);
      options->use_range = true;
    } else if (arg == "--p-end" && i + 1 < argc) {
      options->p_end = std::stod(argv[++i]);
      options->use_range = true;
    } else if (arg == "--p-points" && i + 1 < argc) {
      options->p_points = std::stoi(argv[++i]);
      options->use_range = true;
    } else if (arg == "--p-linear") {
      options->p_log = false;
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
  if (options.blocks == 0) {
    std::cerr << "Blocks must be positive.\n";
    return 1;
  }
  if (options.use_range) {
    if (!(options.p_start > 0.0 && options.p_start < 1.0 &&
          options.p_end > 0.0 && options.p_end <= 1.0 &&
          options.p_end > options.p_start)) {
      std::cerr << "Invalid p range.\n";
      return 1;
    }
    if (options.p_points < 2) {
      std::cerr << "p-points must be at least 2.\n";
      return 1;
    }
  }

  harq::HammingEncoder encoder(options.r);
  harq::HammingDecoder decoder(options.r);
  const int k = encoder.k();
  const int n = encoder.n();
  const std::vector<ErrorOutcome> error_outcomes =
      PrecomputeErrorOutcomes(options.r);

  std::mt19937 rng(options.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);
  std::uniform_real_distribution<double> uni(0.0, 1.0);

  std::cout << "p,ber,bler,ber_theory,bler_theory,bit_errors,block_errors,"
            << "total_bits,total_blocks\n";
  std::cout << std::setprecision(8) << std::fixed;

  std::vector<double> p_values = options.p_list;
  if (options.use_range) {
    p_values.clear();
    if (options.p_log) {
      const double log_start = std::log10(options.p_start);
      const double log_end = std::log10(options.p_end);
      for (int i = 0; i < options.p_points; ++i) {
        double t = static_cast<double>(i) /
            static_cast<double>(options.p_points - 1);
        double value = std::pow(10.0, log_start + t * (log_end - log_start));
        p_values.push_back(value);
      }
    } else {
      for (int i = 0; i < options.p_points; ++i) {
        double t = static_cast<double>(i) /
            static_cast<double>(options.p_points - 1);
        double value = options.p_start + t * (options.p_end - options.p_start);
        p_values.push_back(value);
      }
    }
  }

  for (double p : p_values) {
    long double ber_theory = 0.0L;
    long double bler_theory = 0.0L;
    const long double p_ld = static_cast<long double>(p);
    const long double q_ld = 1.0L - p_ld;
    for (const auto& outcome : error_outcomes) {
      const int w = outcome.weight;
      const long double prob =
          std::pow(p_ld, w) * std::pow(q_ld, n - w);
      ber_theory += static_cast<long double>(outcome.info_errors) * prob;
      if (outcome.block_error) {
        bler_theory += prob;
      }
    }
    ber_theory /= static_cast<long double>(k);

    std::size_t bit_errors = 0;
    std::size_t block_errors = 0;
    std::size_t total_bits = 0;

    for (std::size_t b = 0; b < options.blocks; ++b) {
      std::vector<uint8_t> data(static_cast<std::size_t>(k), 0);
      for (int i = 0; i < k; ++i) {
        data[static_cast<std::size_t>(i)] =
            static_cast<uint8_t>(bit_dist(rng));
      }

      std::vector<uint8_t> codeword = encoder.Encode(data);
      for (uint8_t& bit : codeword) {
        if (uni(rng) < p) {
          bit ^= 1;
        }
      }

      std::vector<uint8_t> decoded = decoder.Decode(codeword);

      bool block_error = false;
      for (int i = 0; i < k; ++i) {
        if (decoded[static_cast<std::size_t>(i)] !=
            data[static_cast<std::size_t>(i)]) {
          ++bit_errors;
          block_error = true;
        }
      }
      if (block_error) {
        ++block_errors;
      }
      total_bits += static_cast<std::size_t>(k);
    }

    double ber = static_cast<double>(bit_errors) /
        static_cast<double>(total_bits);
    double bler = static_cast<double>(block_errors) /
        static_cast<double>(options.blocks);

    std::cout << p << "," << ber << "," << bler << ","
              << static_cast<double>(ber_theory) << ","
              << static_cast<double>(bler_theory) << "," << bit_errors << ","
              << block_errors << "," << total_bits << ","
              << options.blocks << "\n";
  }

  return 0;
}
