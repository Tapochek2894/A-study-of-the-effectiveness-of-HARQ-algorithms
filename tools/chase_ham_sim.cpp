#include "awgn_channel.hpp"
#include "bpsk.hpp"
#include "bpsk_passband.hpp"
#include "chase_algorithm.hpp"
#include "hamming_decoder.hpp"
#include "hamming_encoder.hpp"
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
  std::size_t bits = 600000;
  uint32_t seed = 5489u;
  std::vector<double> snr_list = {-6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4};
  bool use_range = false;
  double snr_start = -6.0;
  double snr_end = 4.0;
  int snr_points = 11;
  int r = 3;
  bool extended = false;
};

void PrintUsage(const char *argv0) {
  std::cout << "Usage: " << argv0 << " [--bits <count>] [--seed <seed>]"
            << " [--snr <dB1,dB2,...>]"
            << " [--snr-start <dB> --snr-end <dB> --snr-points <n>]"
            << " [--r <parity_bits>]\n"
            << " [--extended]\n";
}

bool ParseSnrList(const std::string &value, std::vector<double> *out) {
  std::vector<double> parsed;
  std::stringstream ss(value);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) {
      return false;
    }
    char *endptr = nullptr;
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

bool ParseArgs(int argc, char **argv, Options *options) {
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
    } else if (arg == "--r" && i + 1 < argc) {
      options->r = std::stoi(argv[++i]);
    } else if (arg == "--extended") {
      options->extended = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      return false;
    }
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
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

  if (options.r != 0 && options.r < 2) {
    std::cerr << "r must be >= 2 for Hamming code.\n";
    return 1;
  }

  std::vector<double> snr_values = options.snr_list;
  if (options.use_range) {
    snr_values.clear();
    for (int i = 0; i < options.snr_points; ++i) {
      double t =
          static_cast<double>(i) / static_cast<double>(options.snr_points - 1);
      double value =
          options.snr_start + t * (options.snr_end - options.snr_start);
      snr_values.push_back(value);
    }
  }

  std::mt19937 rng(options.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);

  harq::BpskCarrierConfig config;
  config.carrier_hz = 2.0;
  config.sample_rate_hz = 32.0;
  config.samples_per_symbol = 4;
  config.amplitude = 1.0;
  config.phase = 0.0;

  int k = 0;
  int n = 0;
  if (options.r > 0) {
    int base_n = (1 << options.r) - 1;   // 2^r - 1
    int base_k = base_n - options.r;     // 2^r - 1 - r
    n = base_n;                      // расширенный: 2^r
    k = base_k;   
    if (options.extended) {
      n = base_n + 1; 
    }
    if (options.bits < static_cast<std::size_t>(k)) {
      std::cerr << "bits must be >= k for coded simulation.\n";
      return 1;
    }
  }

  std::size_t info_bits = options.bits;
  if (options.r > 0) {
    info_bits = (options.bits / static_cast<std::size_t>(k)) *
                static_cast<std::size_t>(k);
    if (info_bits == 0) {
      std::cerr << "bits must be >= k for coded simulation.\n";
      return 1;
    }
    if (info_bits != options.bits) {
      std::cerr << "Warning: trimming bits to " << info_bits
                << " to fit k=" << k << ".\n";
    }
  }

  if (options.r > 0) {
    std::cout << "snr_db,ber_uncoded,ber_coded,ber_chase1,ber_chase2,ber_chase3,ber_ml,bit_errors_uncoded,bit_errors_coded,"
              << "bit_errors_chase1,bit_errors_chase2,bit_errors_chase3,bit_errors_ml,total_bits_coded\n";
  } else {
    std::cout << "snr_db,ber,bit_errors,total_bits\n";
  }
  std::cout << std::setprecision(8) << std::fixed;

  harq::BpskModulator modulator;
  harq::BpskDemodulator demodulator;
  std::unique_ptr<harq::HammingEncoder> encoder;
  std::unique_ptr<harq::HammingDecoder> decoder;
  if (options.r > 0) {
    encoder = std::make_unique<harq::HammingEncoder>(options.r);
    decoder = std::make_unique<harq::HammingDecoder>(options.r);
  }

  for (std::size_t idx = 0; idx < snr_values.size(); ++idx) {
    double snr_db = snr_values[idx];
    if (!std::isfinite(snr_db)) {
      std::cerr << "Invalid SNR value: " << snr_db << "\n";
      return 1;
    }
    harq::AwgnChannel channel(snr_db,
                              static_cast<uint32_t>(options.seed + idx));

    std::vector<uint8_t> data(info_bits, 0);
    for (std::size_t i = 0; i < info_bits; ++i) {
      data[i] = static_cast<uint8_t>(bit_dist(rng));
    }

    std::vector<double> symbols = harq::BpskPassbandModulate(data, config);
    std::vector<double> received = channel.AddNoise(symbols);
    std::vector<uint8_t> decoded = harq::BpskPassbandDemodulate(received, config);

    std::size_t bit_errors = 0;
    for (std::size_t i = 0; i < info_bits; ++i) {
      if (decoded[i] != data[i]) {
        ++bit_errors;
      }
    }

    double ber =
        static_cast<double>(bit_errors) / static_cast<double>(info_bits);

    if (options.r == 0) {
      std::cout << snr_db << "," << ber << "," << bit_errors << "," << info_bits
                << "\n";
      continue;
    }

    std::vector<uint8_t> codeword;
    codeword.reserve((info_bits / static_cast<std::size_t>(k)) *
                     static_cast<std::size_t>(n));
    for (std::size_t i = 0; i < info_bits; i += static_cast<std::size_t>(k)) {
      std::vector<uint8_t> block(data.begin() + static_cast<std::ptrdiff_t>(i),
                                 data.begin() +
                                     static_cast<std::ptrdiff_t>(i + k));
      std::vector<uint8_t> encoded;
      if (options.extended) {
        encoded = encoder->EncodeExtended(block);
      } else {
        encoded = encoder->Encode(block);
      }          
      codeword.insert(codeword.end(), encoded.begin(), encoded.end());
    }

    std::vector<double> coded_symbols =
        harq::BpskPassbandModulate(codeword, config);
    std::vector<double> coded_received = channel.AddNoise(coded_symbols);
    std::vector<double> coded_demod =
        harq::BpskPassbandDemodulateSoft(coded_received, config);
    std::vector<uint8_t> coded_demod_hard =
        harq::BpskPassbandDemodulate(coded_received, config);
    
    std::vector<uint8_t> decoded_data;
    std::vector<uint8_t> decoded_data1;
    std::vector<uint8_t> decoded_data2;
    std::vector<uint8_t> decoded_data3;
    std::vector<uint8_t> decoded_data4;
    decoded_data.reserve(info_bits);
    decoded_data1.reserve(info_bits);
    decoded_data2.reserve(info_bits);
    decoded_data3.reserve(info_bits);
    decoded_data4.reserve(info_bits);
    for (std::size_t i = 0; i < coded_demod.size();
         i += static_cast<std::size_t>(n)) {
      std::vector<double> cw(
          coded_demod.begin() + static_cast<std::ptrdiff_t>(i),
          coded_demod.begin() + static_cast<std::ptrdiff_t>(i + n));

      std::vector<uint8_t> cw_hard(
          coded_demod_hard.begin() + static_cast<std::ptrdiff_t>(i),
          coded_demod_hard.begin() + static_cast<std::ptrdiff_t>(i + n));
      std::vector<uint8_t> decoded_block = decoder->Decode(cw_hard);
      decoded_data.insert(decoded_data.end(),
                          decoded_block.begin(),
                          decoded_block.end());

      std::vector<uint8_t> decoded_block1 = harq::DecodeHammingCodesWithChase(
          cw, harq::ProbeAlgorithm::First, harq::HammingDecoder(options.r));
      decoded_data1.insert(decoded_data1.end(), decoded_block1.begin(),
                           decoded_block1.end());
                           
      std::vector<uint8_t> decoded_block2 = harq::DecodeHammingCodesWithChase(
          cw, harq::ProbeAlgorithm::Second, harq::HammingDecoder(options.r));
      decoded_data2.insert(decoded_data2.end(), decoded_block2.begin(),
                           decoded_block2.end());

      std::vector<uint8_t> decoded_block3 = harq::DecodeHammingCodesWithChase(
          cw, harq::ProbeAlgorithm::Third, harq::HammingDecoder(options.r));
      decoded_data3.insert(decoded_data3.end(), decoded_block3.begin(),
                           decoded_block3.end());
      std::vector<uint8_t> decoded_block4 = harq::DecodeHammingML(cw, harq::HammingDecoder(options.r));
      decoded_data4.insert(decoded_data4.end(), decoded_block4.begin(),
                           decoded_block4.end());
    }
    std::size_t coded_errors = 0;
    std::size_t coded_errors1 = 0;
    std::size_t coded_errors2 = 0;
    std::size_t coded_errors3 = 0;
    std::size_t coded_errors4 = 0;
    for (std::size_t i = 0; i < info_bits; ++i) {
      if (decoded_data[i] != data[i]) {
        ++coded_errors;
      }
      if (decoded_data1[i] != data[i]) {
        ++coded_errors1;
      }
      if (decoded_data2[i] != data[i]) {
        ++coded_errors2;
      }
      if (decoded_data3[i] != data[i]) {
        ++coded_errors3;
      }
      if (decoded_data4[i] != data[i]) {
        ++coded_errors4;
      }
    }

    double ber_coded =
        static_cast<double>(coded_errors) / static_cast<double>(info_bits);
    double ber_coded1 =
        static_cast<double>(coded_errors1) / static_cast<double>(info_bits);
    double ber_coded2 =
        static_cast<double>(coded_errors2) / static_cast<double>(info_bits);
    double ber_coded3 =
        static_cast<double>(coded_errors3) / static_cast<double>(info_bits);
    double ber_coded4 =
        static_cast<double>(coded_errors4) / static_cast<double>(info_bits);
    std::cout << snr_db << "," << ber << "," << ber_coded << "," << ber_coded1 << "," << ber_coded2 << ","
     << ber_coded3 << "," << ber_coded4 << "," << bit_errors << "," << coded_errors <<
              "," << coded_errors1 << "," << coded_errors2 << "," << coded_errors3 << "," 
              << coded_errors4 << "," << info_bits  << "\n";
  }

  return 0;
}
