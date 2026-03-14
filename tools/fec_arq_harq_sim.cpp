#include "awgn_channel.hpp"
#include "bpsk.hpp"
#include "crc.hpp"
#include "fec/fec_config.hpp"
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
  std::string codec = "hamming";  // hamming|conv
  int r = 4;                       // Hamming parity bits
  int conv_k = 11;
  int conv_rate_num = 1;
  int conv_rate_den = 2;
  std::string conv_decoder = "viterbi";  // viterbi|bcjr

  std::vector<uint8_t> crc_poly = {1, 0, 1, 1};  // x^3 + x + 1

  int max_retx = 8;
  std::size_t blocks = 20000;
  uint32_t seed = 5489u;

  std::vector<double> snr_list = {-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6};
  bool use_range = false;
  double snr_start = -4.0;
  double snr_end = 6.0;
  int snr_points = 11;
};

void PrintUsage(const char* argv0) {
  std::cerr
      << "Usage: " << argv0 << " [options]\n"
      << "Options:\n"
      << "  --codec <hamming|conv>      FEC backend (default: hamming)\n"
      << "  --r <n>                     Hamming parameter (default: 4 -> (15,11))\n"
      << "  --conv-k <n>                Conv frame K (default: 11)\n"
      << "  --conv-rate <num/den>       Conv rate (default: 1/2)\n"
      << "  --conv-decoder <viterbi|bcjr>\n"
      << "  --crc-poly <1,0,1,1>        CRC polynomial (default: 1,0,1,1)\n"
      << "  --max-retx <n>              Max retransmissions (default: 8)\n"
      << "  --blocks <n>                Number of frames per SNR (default: 20000)\n"
      << "  --seed <n>                  RNG seed (default: 5489)\n"
      << "  --snr <dB,dB,...>           SNR list\n"
      << "  --snr-start <dB> --snr-end <dB> --snr-points <n>\n";
}

bool ParseSnrList(const std::string& s, std::vector<double>* out) {
  out->clear();
  std::stringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    if (tok.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(tok.c_str(), &end);
    if (end == tok.c_str()) return false;
    out->push_back(v);
  }
  return !out->empty();
}

bool ParsePoly(const std::string& s, std::vector<uint8_t>* out) {
  out->clear();
  std::stringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    if (tok.empty()) return false;
    const int c = std::stoi(tok);
    if (c != 0 && c != 1) return false;
    out->push_back(static_cast<uint8_t>(c));
  }
  return out->size() >= 2 && out->front() == 1;
}

bool ParseArgs(int argc, char** argv, Options* opt) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--codec" && i + 1 < argc) {
      opt->codec = argv[++i];
    } else if (a == "--r" && i + 1 < argc) {
      opt->r = std::stoi(argv[++i]);
    } else if (a == "--conv-k" && i + 1 < argc) {
      opt->conv_k = std::stoi(argv[++i]);
    } else if (a == "--conv-rate" && i + 1 < argc) {
      const std::string rate = argv[++i];
      const std::size_t slash = rate.find('/');
      if (slash == std::string::npos) return false;
      opt->conv_rate_num = std::stoi(rate.substr(0, slash));
      opt->conv_rate_den = std::stoi(rate.substr(slash + 1));
    } else if (a == "--conv-decoder" && i + 1 < argc) {
      opt->conv_decoder = argv[++i];
    } else if (a == "--crc-poly" && i + 1 < argc) {
      if (!ParsePoly(argv[++i], &opt->crc_poly)) return false;
    } else if (a == "--max-retx" && i + 1 < argc) {
      opt->max_retx = std::stoi(argv[++i]);
    } else if (a == "--blocks" && i + 1 < argc) {
      opt->blocks = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (a == "--seed" && i + 1 < argc) {
      opt->seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (a == "--snr" && i + 1 < argc) {
      if (!ParseSnrList(argv[++i], &opt->snr_list)) return false;
      opt->use_range = false;
    } else if (a == "--snr-start" && i + 1 < argc) {
      opt->snr_start = std::stod(argv[++i]);
      opt->use_range = true;
    } else if (a == "--snr-end" && i + 1 < argc) {
      opt->snr_end = std::stod(argv[++i]);
      opt->use_range = true;
    } else if (a == "--snr-points" && i + 1 < argc) {
      opt->snr_points = std::stoi(argv[++i]);
      opt->use_range = true;
    } else if (a == "--help" || a == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      return false;
    }
  }
  return true;
}

struct Accum {
  std::size_t bit_errors = 0;
  std::size_t block_errors = 0;
  std::size_t retx_sum = 0;
};

uint32_t SeedFor(uint32_t base,
                 std::size_t snr_idx,
                 std::size_t block_idx,
                 int attempt,
                 uint32_t stream_tag) {
  return base + static_cast<uint32_t>(snr_idx * 1000003u) +
      static_cast<uint32_t>(block_idx * 9176u) +
      static_cast<uint32_t>(attempt * 131u) + stream_tag;
}

void CountErrors(const std::vector<uint8_t>& info_ref,
                 const std::vector<uint8_t>& decoded_frame,
                 std::size_t info_bits,
                 Accum* acc) {
  bool block_error = false;
  for (std::size_t i = 0; i < info_bits; ++i) {
    if (decoded_frame[i] != info_ref[i]) {
      ++acc->bit_errors;
      block_error = true;
    }
  }
  if (block_error) {
    ++acc->block_errors;
  }
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  if (!ParseArgs(argc, argv, &opt)) {
    PrintUsage(argv[0]);
    return 1;
  }

  if (opt.blocks == 0) {
    std::cerr << "blocks must be positive.\n";
    return 1;
  }
  if (opt.max_retx < 0) {
    std::cerr << "max-retx must be >= 0.\n";
    return 1;
  }
  if (opt.use_range) {
    if (opt.snr_points < 2 || !(opt.snr_end > opt.snr_start)) {
      std::cerr << "Invalid SNR range.\n";
      return 1;
    }
  }

  harq::fec::FecConfig cfg{};
  if (opt.codec == "hamming") {
    if (opt.r < 2) {
      std::cerr << "r must be >= 2 for Hamming.\n";
      return 1;
    }
    cfg.codec_type = harq::fec::CodecType::kHamming;
    cfg.hamming_r = opt.r;
  } else if (opt.codec == "conv" || opt.codec == "aff3ct") {
    if (!harq::fec::IsConvolutionalAff3ctAvailable()) {
      std::cerr << "AFF3CT backend is unavailable. Reconfigure with ENABLE_AFF3CT=ON.\n";
      return 1;
    }
    if (opt.conv_k <= 0 || opt.conv_rate_num <= 0 || opt.conv_rate_den <= 0 ||
        opt.conv_rate_num > opt.conv_rate_den) {
      std::cerr << "Invalid conv parameters.\n";
      return 1;
    }
    if (opt.conv_decoder != "viterbi" && opt.conv_decoder != "bcjr") {
      std::cerr << "conv-decoder must be viterbi|bcjr.\n";
      return 1;
    }
    cfg.codec_type = harq::fec::CodecType::kConvolutionalAff3ct;
    cfg.conv_input_bits_per_frame = opt.conv_k;
    cfg.conv_rate_num = opt.conv_rate_num;
    cfg.conv_rate_den = opt.conv_rate_den;
    cfg.conv_decoder = opt.conv_decoder == "bcjr" ? harq::fec::ConvDecoderType::kBcjr
                                                    : harq::fec::ConvDecoderType::kViterbi;
  } else {
    std::cerr << "Unsupported codec: " << opt.codec << "\n";
    return 1;
  }

  auto codec = harq::fec::CreateCodec(cfg);
  const std::size_t k_fec = static_cast<std::size_t>(codec->input_bits_per_frame());
  const std::size_t n_fec = static_cast<std::size_t>(codec->output_bits_per_frame());

  harq::Crc crc(opt.crc_poly);
  const std::size_t crc_bits = static_cast<std::size_t>(crc.r());
  if (crc_bits >= k_fec) {
    std::cerr << "CRC degree is too large for selected codec frame K=" << k_fec << "\n";
    return 1;
  }
  const std::size_t k_info = k_fec - crc_bits;

  std::vector<double> snr_values = opt.snr_list;
  if (opt.use_range) {
    snr_values.clear();
    for (int i = 0; i < opt.snr_points; ++i) {
      const double t = static_cast<double>(i) / static_cast<double>(opt.snr_points - 1);
      snr_values.push_back(opt.snr_start + t * (opt.snr_end - opt.snr_start));
    }
  }

  std::mt19937 rng(opt.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);
  harq::BpskModulator mod;

  std::cout << "snr_db,arq_ber,arq_bler,arq_avg_retx,"
            << "harq_ber,harq_bler,harq_avg_retx,"
            << "codec,k_info,n_fec\n";
  std::cout << std::fixed << std::setprecision(8);

  for (std::size_t snr_idx = 0; snr_idx < snr_values.size(); ++snr_idx) {
    const double snr_db = snr_values[snr_idx];

    Accum arq;
    Accum harq;

    for (std::size_t b = 0; b < opt.blocks; ++b) {
      std::vector<uint8_t> info(k_info, 0);
      for (std::size_t i = 0; i < k_info; ++i) {
        info[i] = static_cast<uint8_t>(bit_dist(rng));
      }

      const std::vector<uint8_t> tx_frame = crc.Encode(info);
      const std::vector<uint8_t> tx_codeword = codec->Encode(tx_frame);
      const std::vector<double> tx_symbols = mod.Modulate(tx_codeword);

      std::vector<uint8_t> arq_decoded_frame(k_fec, 0);
      int arq_attempts = 0;
      for (int attempt = 0; attempt <= opt.max_retx; ++attempt) {
        ++arq_attempts;
        harq::AwgnChannel channel(
            snr_db,
            SeedFor(opt.seed, snr_idx, b, attempt, 17u));
        const std::vector<double> rx = channel.AddNoise(tx_symbols);
        arq_decoded_frame = codec->DecodeSoft(rx);
        if (!crc.HasError(arq_decoded_frame)) {
          break;
        }
      }
      arq.retx_sum += static_cast<std::size_t>(arq_attempts - 1);
      CountErrors(info, arq_decoded_frame, k_info, &arq);

      std::vector<uint8_t> harq_decoded_frame(k_fec, 0);
      std::vector<double> combined_soft(n_fec, 0.0);
      int harq_attempts = 0;
      for (int attempt = 0; attempt <= opt.max_retx; ++attempt) {
        ++harq_attempts;
        harq::AwgnChannel channel(
            snr_db,
            SeedFor(opt.seed, snr_idx, b, attempt, 41u));
        const std::vector<double> rx = channel.AddNoise(tx_symbols);
        for (std::size_t i = 0; i < n_fec; ++i) {
          combined_soft[i] += rx[i];
        }
        harq_decoded_frame = codec->DecodeSoft(combined_soft);
        if (!crc.HasError(harq_decoded_frame)) {
          break;
        }
      }
      harq.retx_sum += static_cast<std::size_t>(harq_attempts - 1);
      CountErrors(info, harq_decoded_frame, k_info, &harq);
    }

    const double total_bits = static_cast<double>(opt.blocks * k_info);
    const double total_blocks = static_cast<double>(opt.blocks);

    const double arq_ber = static_cast<double>(arq.bit_errors) / total_bits;
    const double arq_bler = static_cast<double>(arq.block_errors) / total_blocks;
    const double arq_avg_retx = static_cast<double>(arq.retx_sum) / total_blocks;

    const double harq_ber = static_cast<double>(harq.bit_errors) / total_bits;
    const double harq_bler = static_cast<double>(harq.block_errors) / total_blocks;
    const double harq_avg_retx = static_cast<double>(harq.retx_sum) / total_blocks;

    std::cout << snr_db << "," << arq_ber << "," << arq_bler << "," << arq_avg_retx
              << "," << harq_ber << "," << harq_bler << "," << harq_avg_retx
              << "," << opt.codec << "," << k_info << "," << n_fec << "\n";
  }

  return 0;
}
