#include "bpsk.hpp"
#include "crc.hpp"
#include "fec/fec_config.hpp"
#include "fec/fec_factory.hpp"
#include "rayleigh_channel.hpp"

#include <algorithm>
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

// 3GPP TS 38.212 / 36.212 §5.1.1:
//   g_CRC24A(D) = D^24 + D^23 + D^18 + D^17 + D^14 + D^11 + D^10
//               + D^7 + D^6 + D^5 + D^4 + D^3 + D + 1
const std::vector<uint8_t> kCrc24aPolynomial = {
    1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0,
    1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1};

// g_CRC16(D) = D^16 + D^12 + D^5 + 1 (CCITT, 3GPP CRC-16).
const std::vector<uint8_t> kCrc16Polynomial = {
    1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

struct Options {
  std::size_t blocks = 50000;
  uint32_t seed = 5489u;
  std::size_t info_bits = 512;
  std::string crc_preset = "24a";  // 24a|16
  std::vector<uint8_t> crc_polynomial = kCrc24aPolynomial;
  std::string mode = "both";  // uncoded|coded|both
  int conv_rate_num = 1;
  int conv_rate_den = 2;
  std::string conv_decoder = "viterbi";  // viterbi|bcjr
  int block_size = 1;
  std::vector<double> snr_list = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20};
  bool use_range = false;
  double snr_start = 0.0;
  double snr_end = 20.0;
  int snr_points = 11;
};

void PrintUsage(const char* argv0) {
  std::cout
      << "Usage: " << argv0
      << " [--blocks <count>] [--seed <seed>] [--info-bits <k>]"
      << " [--crc <24a|16>] [--mode <uncoded|coded|both>]"
      << " [--conv-rate <num/den>] [--conv-decoder <viterbi|bcjr>]"
      << " [--block-size <symbols>]"
      << " [--snr <dB1,dB2,...>]"
      << " [--snr-start <dB> --snr-end <dB> --snr-points <n>]\n"
      << "\nMetrics per SNR (CSV):\n"
      << "  uncoded:  p_undetected_unc, p_detected_unc, p_correct_unc\n"
      << "  coded:    p_undetected_cod, p_detected_cod, p_correct_cod\n"
      << "Modes: uncoded — только без свёрточного кода; coded — только с ним;\n"
      << "       both (по умолчанию) — обе кривые в одном CSV.\n";
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

bool ParseArgs(int argc, char** argv, Options* options) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--blocks" && i + 1 < argc) {
      options->blocks = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      options->seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--info-bits" && i + 1 < argc) {
      options->info_bits = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (arg == "--crc" && i + 1 < argc) {
      options->crc_preset = argv[++i];
      if (options->crc_preset == "24a") {
        options->crc_polynomial = kCrc24aPolynomial;
      } else if (options->crc_preset == "16") {
        options->crc_polynomial = kCrc16Polynomial;
      } else {
        std::cerr << "Unsupported --crc value: " << options->crc_preset << "\n";
        return false;
      }
    } else if (arg == "--mode" && i + 1 < argc) {
      options->mode = argv[++i];
      if (options->mode != "uncoded" && options->mode != "coded" &&
          options->mode != "both") {
        std::cerr << "Invalid --mode value.\n";
        return false;
      }
    } else if (arg == "--conv-rate" && i + 1 < argc) {
      const std::string rate = argv[++i];
      const std::size_t slash = rate.find('/');
      if (slash == std::string::npos) return false;
      options->conv_rate_num = std::stoi(rate.substr(0, slash));
      options->conv_rate_den = std::stoi(rate.substr(slash + 1));
    } else if (arg == "--conv-decoder" && i + 1 < argc) {
      options->conv_decoder = argv[++i];
    } else if (arg == "--block-size" && i + 1 < argc) {
      options->block_size = std::stoi(argv[++i]);
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
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return false;
    }
  }
  return true;
}

// Итоги по одному блоку.
struct BlockOutcome {
  bool detected = false;     // CRC обнаружил ошибку (синдром != 0).
  bool undetected = false;   // CRC прошёл, но информационная часть искажена.
  bool correct = false;      // CRC прошёл и информационная часть совпала.
};

BlockOutcome ClassifyBlock(const harq::Crc& crc,
                           const std::vector<uint8_t>& message,
                           const std::vector<uint8_t>& decoded_codeword) {
  BlockOutcome o;
  if (crc.HasError(decoded_codeword)) {
    o.detected = true;
    return o;
  }
  std::vector<uint8_t> decoded_info = crc.Decode(decoded_codeword);
  const std::size_t k = message.size();
  for (std::size_t i = 0; i < k; ++i) {
    if (decoded_info[i] != message[i]) {
      o.undetected = true;
      return o;
    }
  }
  o.correct = true;
  return o;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!ParseArgs(argc, argv, &options)) {
    PrintUsage(argv[0]);
    return 1;
  }
  if (options.blocks == 0) {
    std::cerr << "blocks must be positive.\n";
    return 1;
  }
  if (options.info_bits == 0) {
    std::cerr << "info-bits must be positive.\n";
    return 1;
  }
  if (options.block_size < 1) {
    std::cerr << "block-size must be >= 1.\n";
    return 1;
  }
  if (options.use_range) {
    if (options.snr_points < 2 || !(options.snr_end > options.snr_start)) {
      std::cerr << "Invalid SNR range.\n";
      return 1;
    }
  }
  const bool need_coded = options.mode == "coded" || options.mode == "both";
  const bool need_uncoded = options.mode == "uncoded" || options.mode == "both";

  if (need_coded && !harq::fec::IsConvolutionalAff3ctAvailable()) {
    std::cerr << "AFF3CT backend is unavailable. Rebuild with ENABLE_AFF3CT=ON.\n";
    return 1;
  }

  std::vector<double> snr_values = options.snr_list;
  if (options.use_range) {
    snr_values.clear();
    for (int i = 0; i < options.snr_points; ++i) {
      double t = static_cast<double>(i) /
          static_cast<double>(options.snr_points - 1);
      snr_values.push_back(
          options.snr_start + t * (options.snr_end - options.snr_start));
    }
  }

  harq::Crc crc(options.crc_polynomial);
  const int r = crc.r();
  const std::size_t k = options.info_bits;
  const std::size_t n_crc = k + static_cast<std::size_t>(r);

  std::unique_ptr<harq::fec::IFecCodec> conv_codec;
  if (need_coded) {
    harq::fec::FecConfig fec_config{};
    fec_config.codec_type = harq::fec::CodecType::kConvolutionalAff3ct;
    fec_config.conv_input_bits_per_frame = static_cast<int>(n_crc);
    fec_config.conv_rate_num = options.conv_rate_num;
    fec_config.conv_rate_den = options.conv_rate_den;
    fec_config.conv_decoder =
        options.conv_decoder == "bcjr" ? harq::fec::ConvDecoderType::kBcjr
                                        : harq::fec::ConvDecoderType::kViterbi;
    conv_codec = harq::fec::CreateCodec(fec_config);
    if (static_cast<std::size_t>(conv_codec->input_bits_per_frame()) != n_crc) {
      std::cerr << "AFF3CT changed frame size; expected " << n_crc << ", got "
                << conv_codec->input_bits_per_frame() << ".\n";
      return 1;
    }
  }
  const int n_coded = conv_codec ? conv_codec->output_bits_per_frame() : 0;

  std::cerr << "CRC: preset=" << options.crc_preset
            << ", r=" << r << ", k=" << k << ", n_crc=" << n_crc << "\n";
  if (need_coded) {
    std::cerr << "Conv: rate=" << options.conv_rate_num << "/"
              << options.conv_rate_den
              << ", decoder=" << options.conv_decoder
              << ", N_frame=" << n_coded << "\n";
  }
  std::cerr << "Channel: BPSK + Rayleigh, block_size=" << options.block_size
            << ", blocks/SNR=" << options.blocks << "\n";

  std::cout << "snr_db";
  if (need_uncoded) {
    std::cout << ",p_undetected_unc,p_detected_unc,p_correct_unc"
              << ",undetected_unc,detected_unc,correct_unc";
  }
  if (need_coded) {
    std::cout << ",p_undetected_cod,p_detected_cod,p_correct_cod"
              << ",undetected_cod,detected_cod,correct_cod";
  }
  std::cout << ",total_blocks\n";
  std::cout << std::setprecision(10) << std::scientific;

  std::mt19937 rng(options.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);
  harq::BpskModulator modulator;
  harq::BpskDemodulator demodulator;

  for (std::size_t idx = 0; idx < snr_values.size(); ++idx) {
    const double snr_db = snr_values[idx];
    if (!std::isfinite(snr_db)) {
      std::cerr << "Invalid SNR value: " << snr_db << "\n";
      return 1;
    }
    harq::RayleighChannel channel(
        snr_db, static_cast<uint32_t>(options.seed + idx + 1),
        options.block_size);

    std::size_t undetected_unc = 0, detected_unc = 0, correct_unc = 0;
    std::size_t undetected_cod = 0, detected_cod = 0, correct_cod = 0;

    for (std::size_t b = 0; b < options.blocks; ++b) {
      std::vector<uint8_t> message(k, 0);
      for (std::size_t i = 0; i < k; ++i) {
        message[i] = static_cast<uint8_t>(bit_dist(rng));
      }
      std::vector<uint8_t> crc_codeword = crc.Encode(message);

      if (need_uncoded) {
        std::vector<double> symbols = modulator.Modulate(crc_codeword);
        std::vector<double> received = channel.AddNoise(symbols);
        // Когерентный приём с известным μ: знак μ·r = знак r,
        // поэтому жёсткое решение по знаку r остаётся корректным.
        std::vector<uint8_t> hard = demodulator.Demodulate(received);
        BlockOutcome o = ClassifyBlock(crc, message, hard);
        if (o.undetected) ++undetected_unc;
        else if (o.detected) ++detected_unc;
        else ++correct_unc;
      }

      if (need_coded) {
        std::vector<uint8_t> conv_encoded = conv_codec->Encode(crc_codeword);
        std::vector<double> symbols = modulator.Modulate(conv_encoded);
        std::vector<double> received = channel.AddNoise(symbols);
        std::vector<double> llr =
            channel.ComputeLlr(received, channel.last_fading_amplitudes());
        std::vector<uint8_t> decoded_codeword = conv_codec->DecodeSoft(llr);
        BlockOutcome o = ClassifyBlock(crc, message, decoded_codeword);
        if (o.undetected) ++undetected_cod;
        else if (o.detected) ++detected_cod;
        else ++correct_cod;
      }
    }

    const double total = static_cast<double>(options.blocks);
    std::cout << snr_db;
    if (need_uncoded) {
      std::cout << "," << undetected_unc / total
                << "," << detected_unc / total
                << "," << correct_unc / total
                << "," << undetected_unc
                << "," << detected_unc
                << "," << correct_unc;
    }
    if (need_coded) {
      std::cout << "," << undetected_cod / total
                << "," << detected_cod / total
                << "," << correct_cod / total
                << "," << undetected_cod
                << "," << detected_cod
                << "," << correct_cod;
    }
    std::cout << "," << options.blocks << "\n";
  }

  return 0;
}
