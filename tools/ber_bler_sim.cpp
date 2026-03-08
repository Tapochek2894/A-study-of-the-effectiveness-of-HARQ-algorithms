#include "fec/fec_factory.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
  int r = 3;
  std::size_t blocks = 10000;
  uint32_t seed = 5489u;
  std::string codec = "hamming";
  int conv_k = 1024;
  int conv_rate_num = 1;
  int conv_rate_den = 2;
  std::string conv_decoder = "viterbi";
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

std::vector<ErrorOutcome> PrecomputeErrorOutcomes(const harq::fec::IFecCodec& codec) {
  const int n = codec.output_bits_per_frame();
  const int k = codec.input_bits_per_frame();
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

    std::vector<uint8_t> decoded = codec.DecodeHard(error_vec);
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
            << " [--codec <hamming|conv>] [--r <parity_bits>] [--blocks <count>] [--seed <seed>]"
            << " [--conv-k <bits>] [--conv-rate <num/den>] [--conv-decoder <viterbi|bcjr>]"
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

bool ParseRate(const std::string& value, int* num, int* den) {
  const std::size_t slash = value.find('/');
  if (slash == std::string::npos || slash == 0 || slash + 1 >= value.size()) {
    return false;
  }
  const std::string lhs = value.substr(0, slash);
  const std::string rhs = value.substr(slash + 1);
  try {
    int parsed_num = std::stoi(lhs);
    int parsed_den = std::stoi(rhs);
    if (parsed_num <= 0 || parsed_den <= 0 || parsed_num > parsed_den) {
      return false;
    }
    *num = parsed_num;
    *den = parsed_den;
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseArgs(int argc, char** argv, Options* options) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--codec" && i + 1 < argc) {
      options->codec = argv[++i];
    } else if (arg == "--r" && i + 1 < argc) {
      options->r = std::stoi(argv[++i]);
    } else if (arg == "--blocks" && i + 1 < argc) {
      options->blocks = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      options->seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--conv-k" && i + 1 < argc) {
      options->conv_k = std::stoi(argv[++i]);
    } else if (arg == "--conv-rate" && i + 1 < argc) {
      if (!ParseRate(argv[++i], &options->conv_rate_num, &options->conv_rate_den)) {
        return false;
      }
    } else if (arg == "--conv-decoder" && i + 1 < argc) {
      options->conv_decoder = argv[++i];
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
  if (options.codec != "hamming" && options.codec != "conv") {
    std::cerr << "codec must be one of: hamming, conv.\n";
    return 1;
  }
  if (options.codec == "hamming" && options.r < 2) {
    std::cerr << "r must be >= 2 for Hamming code.\n";
    return 1;
  }
  if (options.codec == "conv" &&
      options.conv_decoder != "viterbi" &&
      options.conv_decoder != "bcjr") {
    std::cerr << "conv-decoder must be one of: viterbi, bcjr.\n";
    return 1;
  }
  if (options.codec == "conv" &&
      (options.conv_k <= 0 ||
       options.conv_rate_num <= 0 ||
       options.conv_rate_den <= 0 ||
       options.conv_rate_num > options.conv_rate_den)) {
    std::cerr << "Invalid conv settings.\n";
    return 1;
  }
  if (options.codec == "conv" && !harq::fec::IsConvolutionalAff3ctAvailable()) {
    std::cerr
        << "Convolutional AFF3CT backend is unavailable in this build. "
        << "Rebuild with -DENABLE_AFF3CT=ON and installed AFF3CT.\n";
    return 2;
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

  harq::fec::FecConfig config;
  config.codec_type =
      options.codec == "hamming"
          ? harq::fec::CodecType::kHamming
          : harq::fec::CodecType::kConvolutionalAff3ct;
  config.hamming_r = options.r;
  config.conv_input_bits_per_frame = options.conv_k;
  config.conv_rate_num = options.conv_rate_num;
  config.conv_rate_den = options.conv_rate_den;
  config.conv_decoder = options.conv_decoder == "bcjr"
                            ? harq::fec::ConvDecoderType::kBcjr
                            : harq::fec::ConvDecoderType::kViterbi;

  std::unique_ptr<harq::fec::IFecCodec> codec = harq::fec::CreateCodec(config);
  const int k = codec->input_bits_per_frame();
  const int n = codec->output_bits_per_frame();
  const std::vector<ErrorOutcome> error_outcomes =
      PrecomputeErrorOutcomes(*codec);

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

      std::vector<uint8_t> codeword = codec->Encode(data);
      for (uint8_t& bit : codeword) {
        if (uni(rng) < p) {
          bit ^= 1;
        }
      }

      std::vector<uint8_t> decoded = codec->DecodeHard(codeword);

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
