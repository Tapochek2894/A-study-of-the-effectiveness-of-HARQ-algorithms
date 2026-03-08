#include "awgn_channel.hpp"
#include "fec/fec_factory.hpp"
#include "qpsk.hpp"

#include <cmath>
#include <complex>
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
  std::vector<double> snr_list = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  bool use_range = false;
  double snr_start = 0.0;
  double snr_end = 12.0;
  int snr_points = 13;
  int r = 0;
  std::string codec = "hamming";
  int conv_k = 1024;
  int conv_rate_num = 1;
  int conv_rate_den = 2;
  std::string conv_decoder = "viterbi";
};

void PrintUsage(const char* argv0) {
  std::cout << "Usage: " << argv0
            << " [--bits <count>] [--seed <seed>]"
            << " [--snr <dB1,dB2,...>]"
            << " [--snr-start <dB> --snr-end <dB> --snr-points <n>]"
            << " [--r <parity_bits>]"
            << " [--codec <hamming|conv>]"
            << " [--conv-k <bits>]"
            << " [--conv-rate <num/den>]"
            << " [--conv-decoder <viterbi|bcjr>]\n";
}

bool ParseSnrList(const std::string& value, std::vector<double>* out) {
  std::vector<double> parsed;
  std::stringstream ss(value);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) return false;
    char* endptr = nullptr;
    double v = std::strtod(token.c_str(), &endptr);
    if (endptr == token.c_str()) return false;
    parsed.push_back(v);
  }
  if (parsed.empty()) return false;
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
    if (arg == "--bits" && i + 1 < argc) {
      options->bits = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      options->seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--snr" && i + 1 < argc) {
      if (!ParseSnrList(argv[++i], &options->snr_list)) return false;
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
    } else if (arg == "--codec" && i + 1 < argc) {
      options->codec = argv[++i];
    } else if (arg == "--conv-k" && i + 1 < argc) {
      options->conv_k = std::stoi(argv[++i]);
    } else if (arg == "--conv-rate" && i + 1 < argc) {
      if (!ParseRate(argv[++i], &options->conv_rate_num, &options->conv_rate_den)) {
        return false;
      }
    } else if (arg == "--conv-decoder" && i + 1 < argc) {
      options->conv_decoder = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      return false;
    }
  }
  return true;
}

inline uint8_t LlrToBit(double llr) {
  return (llr >= 0.0) ? 1 : 0;
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
  const bool coded_mode_requested = (options.codec == "conv" || options.r > 0);

  if (options.codec != "hamming" && options.codec != "conv") {
    std::cerr << "codec must be one of: hamming, conv.\n";
    return 1;
  }
  if (options.codec == "hamming" && options.r != 0 && options.r < 2) {
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

  std::vector<double> snr_values = options.snr_list;
  if (options.use_range) {
    snr_values.clear();
    for (int i = 0; i < options.snr_points; ++i) {
      double t = static_cast<double>(i) /
          static_cast<double>(options.snr_points - 1);
      snr_values.push_back(options.snr_start + 
                          t * (options.snr_end - options.snr_start));
    }
  }

  std::mt19937 rng(options.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);

  harq::QpskModulator modulator;
  harq::QpskDemodulator demodulator;

  try {
    int k = 0;
    int n = 0;
    std::unique_ptr<harq::fec::IFecCodec> codec;
    if (coded_mode_requested) {
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
      codec = harq::fec::CreateCodec(config);

      k = codec->input_bits_per_frame();
      n = codec->output_bits_per_frame();
      if (options.bits < static_cast<std::size_t>(k)) {
        std::cerr << "bits must be >= input_bits_per_frame for coded simulation.\n";
        return 1;
      }
    }

    std::size_t info_bits = options.bits;
    if (coded_mode_requested) {
      info_bits = (options.bits / static_cast<std::size_t>(k)) *
                  static_cast<std::size_t>(k);
      if (info_bits == 0) {
        std::cerr << "bits must be >= input_bits_per_frame for coded simulation.\n";
        return 1;
      }
      if (info_bits != options.bits) {
        std::cerr << "Warning: trimming bits to " << info_bits
                  << " to fit input_bits_per_frame=" << k << ".\n";
      }
    }

    if (coded_mode_requested) {
      std::cout << "snr_db,ber_uncoded,ber_coded,bit_errors_uncoded,"
                << "bit_errors_coded,total_bits_uncoded,total_bits_coded\n";
    } else {
      std::cout << "snr_db,ber,bit_errors,total_bits\n";
    }
    std::cout << std::setprecision(8) << std::fixed;

    for (std::size_t idx = 0; idx < snr_values.size(); ++idx) {
      double snr_db = snr_values[idx];
      if (!std::isfinite(snr_db)) {
        std::cerr << "Invalid SNR value: " << snr_db << "\n";
        return 1;
      }
    
    harq::AwgnChannel channel(snr_db, 
                              static_cast<uint32_t>(options.seed + idx));

    // Генерация данных
    std::vector<uint8_t> data(info_bits, 0);
    for (std::size_t i = 0; i < info_bits; ++i) {
      data[i] = static_cast<uint8_t>(bit_dist(rng));
    }

    // === UNCODED QPSK ===
    // QPSK требует чётного числа бит
    std::size_t uncoded_bits = info_bits - (info_bits % 2);
    if (uncoded_bits != info_bits) {
      std::cerr << "Warning: trimmed 1 bit for QPSK alignment (uncoded).\n";
    }
    
    std::vector<uint8_t> uncoded_data(data.begin(), 
                                      data.begin() + uncoded_bits);
    
    // Модуляция: bits → complex symbols
    std::vector<std::complex<double>> symbols = modulator.Modulate(uncoded_data);
    
    std::vector<std::complex<double>> received = channel.TransmitComplex(symbols);
    
    // Демодуляция: complex symbols → LLR (soft values)
    std::vector<double> llrs = demodulator.Demodulate(received, snr_db);
    
    // Жёсткие решения для BER: LLR → bit
    std::vector<uint8_t> decoded;
    decoded.reserve(llrs.size());
    for (double llr : llrs) {
      decoded.push_back(LlrToBit(llr));
    }

    std::size_t bit_errors = 0;
    for (std::size_t i = 0; i < uncoded_bits; ++i) {
      if (decoded[i] != uncoded_data[i]) ++bit_errors;
    }
    double ber = static_cast<double>(bit_errors) / uncoded_bits;

      if (!coded_mode_requested) {
        std::cout << snr_db << "," << ber << "," << bit_errors << ","
                  << uncoded_bits << "\n";
        continue;
      }
    std::vector<uint8_t> codeword;
    std::size_t num_blocks = info_bits / static_cast<std::size_t>(k);
    codeword.reserve(num_blocks * static_cast<std::size_t>(n));
    
    for (std::size_t i = 0; i < info_bits; i += static_cast<std::size_t>(k)) {
      std::vector<uint8_t> block(data.begin() + i, data.begin() + i + k);
      std::vector<uint8_t> encoded = codec->Encode(block);
      codeword.insert(codeword.end(), encoded.begin(), encoded.end());
    }

    bool dummy_added = false;
    if (codeword.size() % 2 != 0) {
      codeword.push_back(0);  // dummy bit
      dummy_added = true;
    }
    
    std::vector<std::complex<double>> coded_symbols = modulator.Modulate(codeword);
    std::vector<std::complex<double>> coded_received = channel.TransmitComplex(coded_symbols);
    std::vector<double> coded_llrs = demodulator.Demodulate(coded_received, snr_db);
    
    std::vector<uint8_t> coded_hard;
    coded_hard.reserve(coded_llrs.size());
    for (double llr : coded_llrs) {
      coded_hard.push_back(LlrToBit(llr));
    }
    
    if (dummy_added && !coded_hard.empty()) {
      coded_hard.pop_back();
    }

    std::vector<uint8_t> decoded_data;
    decoded_data.reserve(info_bits);
    
    for (std::size_t i = 0; i + n <= coded_hard.size(); i += n) {
      std::vector<uint8_t> cw(coded_hard.begin() + i, 
                              coded_hard.begin() + i + n);
      std::vector<uint8_t> decoded_block = codec->DecodeHard(cw);
      decoded_data.insert(decoded_data.end(), 
                          decoded_block.begin(), 
                          decoded_block.end());
    }

    std::size_t coded_errors = 0;
    std::size_t compared_bits = std::min(info_bits, decoded_data.size());
    for (std::size_t i = 0; i < compared_bits; ++i) {
      if (decoded_data[i] != data[i]) ++coded_errors;
    }
    double ber_coded = static_cast<double>(coded_errors) / compared_bits;

    // Вывод результатов
    std::cout << snr_db << "," << ber << "," << ber_coded << ","
              << bit_errors << "," << coded_errors << "," 
              << uncoded_bits << "," << compared_bits << "\n";
    }
  } catch (const std::exception& e) {
    std::cerr << "Simulation failed: " << e.what() << "\n";
    return 2;
  }

  return 0;
}
