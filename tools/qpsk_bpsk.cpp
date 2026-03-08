#include "awgn_channel.hpp"
#include "bpsk.hpp"
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
  int r = 0;  // Hamming parity bits (0 = uncoded)
  std::string codec = "hamming";
  int conv_k = 1024;
  int conv_rate_num = 1;
  int conv_rate_den = 2;
  std::string conv_decoder = "viterbi";
  
  // Flags for modulation schemes
  bool run_bpsk = true;
  bool run_qpsk = true;
};

void PrintUsage(const char* argv0) {
  std::cout << "Usage: " << argv0
            << " [--bits <count>] [--seed <seed>]"
            << " [--snr <dB1,dB2,...>]"
            << " [--snr-start <dB> --snr-end <dB> --snr-points <n>]"
            << " [--r <parity_bits>]"
            << " [--codec <hamming|conv>] [--conv-k <bits>]"
            << " [--conv-rate <num/den>] [--conv-decoder <viterbi|bcjr>]"
            << " [--bpsk] [--qpsk] [--both]\n"
            << "  --bpsk   Run BPSK only (default: both)\n"
            << "  --qpsk   Run QPSK only (default: both)\n"
            << "  --both   Run both BPSK and QPSK (default)\n";
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
  bool bpsk_set = false;
  bool qpsk_set = false;
  
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
    } else if (arg == "--bpsk") {
      options->run_bpsk = true;
      options->run_qpsk = false;
      bpsk_set = true;
    } else if (arg == "--qpsk") {
      options->run_bpsk = false;
      options->run_qpsk = true;
      qpsk_set = true;
    } else if (arg == "--both") {
      options->run_bpsk = true;
      options->run_qpsk = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      return false;
    }
  }
  
  // Default: both if neither specified
  if (!bpsk_set && !qpsk_set) {
    options->run_bpsk = true;
    options->run_qpsk = true;
  }
  
  return true;
}

// Convert LLR to hard bit decision
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

  // Build SNR list
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

  // RNG setup
  std::mt19937 rng(options.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);

  // FEC frame parameters
  int k = 0, n = 0;
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

  // Adjust info_bits to fit code blocks
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

  // Print CSV header based on selected modulations
    std::cout << "snr_db";
  if (options.run_bpsk) {
    if (coded_mode_requested) {
      std::cout << ",bpsk_ber_uncoded,bpsk_ber_coded"
                << ",bpsk_errors_uncoded,bpsk_errors_coded";
    } else {
      std::cout << ",bpsk_ber,bpsk_errors";
    }
  }
  if (options.run_qpsk) {
    if (coded_mode_requested) {
      std::cout << ",qpsk_ber_uncoded,qpsk_ber_coded"
                << ",qpsk_errors_uncoded,qpsk_errors_coded";
    } else {
      std::cout << ",qpsk_ber,qpsk_errors";
    }
  }
  std::cout << ",total_bits\n";
  
  std::cout << std::setprecision(8) << std::fixed;

  // Initialize modulators/demodulators
  std::unique_ptr<harq::BpskModulator> bpsk_mod;
  std::unique_ptr<harq::BpskDemodulator> bpsk_demod;
  std::unique_ptr<harq::QpskModulator> qpsk_mod;
  std::unique_ptr<harq::QpskDemodulator> qpsk_demod;
  
  if (options.run_bpsk) {
    bpsk_mod = std::make_unique<harq::BpskModulator>();
    bpsk_demod = std::make_unique<harq::BpskDemodulator>();
  }
  if (options.run_qpsk) {
    qpsk_mod = std::make_unique<harq::QpskModulator>();
    qpsk_demod = std::make_unique<harq::QpskDemodulator>();
  }

  // Main simulation loop
  for (std::size_t idx = 0; idx < snr_values.size(); ++idx) {
    double snr_db = snr_values[idx];
    if (!std::isfinite(snr_db)) {
      std::cerr << "Invalid SNR value: " << snr_db << "\n";
      return 1;
    }
    
    // Channel (same noise realization for fair comparison if seed is same)
    harq::AwgnChannel channel(snr_db, static_cast<uint32_t>(options.seed + idx));

    // Generate random data
    std::vector<uint8_t> data(info_bits, 0);
    for (std::size_t i = 0; i < info_bits; ++i) {
      data[i] = static_cast<uint8_t>(bit_dist(rng));
    }

    // === BPSK Simulation ===
    std::size_t bpsk_errors_uncoded = 0;
    std::size_t bpsk_errors_coded = 0;
    double bpsk_ber_uncoded = 0.0;
    double bpsk_ber_coded = 0.0;
    std::size_t bpsk_bits = info_bits;
    
    if (options.run_bpsk) {
      // Uncoded BPSK
      std::vector<double> bpsk_symbols = bpsk_mod->Modulate(data);
      std::vector<double> bpsk_received = channel.AddNoise(bpsk_symbols);
      std::vector<uint8_t> bpsk_decoded = bpsk_demod->Demodulate(bpsk_received);
      
      for (std::size_t i = 0; i < info_bits; ++i) {
        if (bpsk_decoded[i] != data[i]) ++bpsk_errors_uncoded;
      }
      bpsk_ber_uncoded = static_cast<double>(bpsk_errors_uncoded) / info_bits;

      // Coded BPSK (if enabled)
      if (coded_mode_requested) {
        std::vector<uint8_t> codeword;
        std::size_t num_blocks = info_bits / static_cast<std::size_t>(k);
        codeword.reserve(num_blocks * static_cast<std::size_t>(n));
        
        for (std::size_t i = 0; i < info_bits; i += static_cast<std::size_t>(k)) {
          std::vector<uint8_t> block(data.begin() + i, data.begin() + i + k);
          std::vector<uint8_t> encoded = codec->Encode(block);
          codeword.insert(codeword.end(), encoded.begin(), encoded.end());
        }
        
        std::vector<double> coded_symbols = bpsk_mod->Modulate(codeword);
        std::vector<double> coded_received = channel.AddNoise(coded_symbols);
        std::vector<uint8_t> coded_demod = bpsk_demod->Demodulate(coded_received);
        
        std::vector<uint8_t> decoded_data;
        decoded_data.reserve(info_bits);
        for (std::size_t i = 0; i + n <= coded_demod.size(); i += n) {
          std::vector<uint8_t> cw(coded_demod.begin() + i, 
                                  coded_demod.begin() + i + n);
          std::vector<uint8_t> decoded_block = codec->DecodeHard(cw);
          decoded_data.insert(decoded_data.end(), 
                              decoded_block.begin(), 
                              decoded_block.end());
        }
        
        std::size_t compared = std::min(info_bits, decoded_data.size());
        for (std::size_t i = 0; i < compared; ++i) {
          if (decoded_data[i] != data[i]) ++bpsk_errors_coded;
        }
        bpsk_ber_coded = static_cast<double>(bpsk_errors_coded) / compared;
      }
    }

    // === QPSK Simulation ===
    std::size_t qpsk_errors_uncoded = 0;
    std::size_t qpsk_errors_coded = 0;
    double qpsk_ber_uncoded = 0.0;
    double qpsk_ber_coded = 0.0;
    std::size_t qpsk_bits = info_bits;
    
    if (options.run_qpsk) {
      // QPSK requires even number of bits
      std::size_t qpsk_info_bits = info_bits - (info_bits % 2);
      if (qpsk_info_bits != info_bits) {
        std::cerr << "Warning: trimmed 1 bit for QPSK alignment.\n";
      }
      qpsk_bits = qpsk_info_bits;
      
      std::vector<uint8_t> qpsk_data(data.begin(), data.begin() + qpsk_info_bits);
      
      // Uncoded QPSK
      std::vector<std::complex<double>> qpsk_symbols = qpsk_mod->Modulate(qpsk_data);
      std::vector<std::complex<double>> qpsk_received = channel.TransmitComplex(qpsk_symbols);
      std::vector<double> qpsk_llrs = qpsk_demod->Demodulate(qpsk_received, snr_db);
      
      // LLR -> hard bits
      std::vector<uint8_t> qpsk_decoded;
      qpsk_decoded.reserve(qpsk_llrs.size());
      for (double llr : qpsk_llrs) {
        qpsk_decoded.push_back(LlrToBit(llr));
      }
      
      for (std::size_t i = 0; i < qpsk_info_bits; ++i) {
        if (qpsk_decoded[i] != qpsk_data[i]) ++qpsk_errors_uncoded;
      }
      qpsk_ber_uncoded = static_cast<double>(qpsk_errors_uncoded) / qpsk_info_bits;

      // Coded QPSK (if enabled)
      if (coded_mode_requested) {
        std::vector<uint8_t> codeword;
        std::size_t num_blocks = info_bits / static_cast<std::size_t>(k);
        codeword.reserve(num_blocks * static_cast<std::size_t>(n));
        
        for (std::size_t i = 0; i < info_bits; i += static_cast<std::size_t>(k)) {
          std::vector<uint8_t> block(data.begin() + i, data.begin() + i + k);
          std::vector<uint8_t> encoded = codec->Encode(block);
          codeword.insert(codeword.end(), encoded.begin(), encoded.end());
        }
        
        // Align to even for QPSK
        bool dummy_added = false;
        if (codeword.size() % 2 != 0) {
          codeword.push_back(0);
          dummy_added = true;
        }
        
        std::vector<std::complex<double>> coded_symbols = qpsk_mod->Modulate(codeword);
        std::vector<std::complex<double>> coded_received = channel.TransmitComplex(coded_symbols);
        std::vector<double> coded_llrs = qpsk_demod->Demodulate(coded_received, snr_db);
        
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
        
        std::size_t compared = std::min(info_bits, decoded_data.size());
        for (std::size_t i = 0; i < compared; ++i) {
          if (decoded_data[i] != data[i]) ++qpsk_errors_coded;
        }
        qpsk_ber_coded = static_cast<double>(qpsk_errors_coded) / compared;
      }
    }

    // === Output CSV Row ===
    std::cout << snr_db;
    if (options.run_bpsk) {
      if (coded_mode_requested) {
        std::cout << "," << bpsk_ber_uncoded << "," << bpsk_ber_coded
                  << "," << bpsk_errors_uncoded << "," << bpsk_errors_coded;
      } else {
        std::cout << "," << bpsk_ber_uncoded << "," << bpsk_errors_uncoded;
      }
    }
    if (options.run_qpsk) {
      if (coded_mode_requested) {
        std::cout << "," << qpsk_ber_uncoded << "," << qpsk_ber_coded
                  << "," << qpsk_errors_uncoded << "," << qpsk_errors_coded;
      } else {
        std::cout << "," << qpsk_ber_uncoded << "," << qpsk_errors_uncoded;
      }
    }
    std::cout << "," << info_bits << "\n";
  }

  return 0;
}
