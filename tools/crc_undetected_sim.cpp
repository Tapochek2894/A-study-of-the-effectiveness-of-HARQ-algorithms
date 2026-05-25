#include "bpsk.hpp"
#include "crc.hpp"
#include "fec/fec_config.hpp"
#include "fec/fec_factory.hpp"
#include "rayleigh_channel.hpp"

#include <algorithm>
#include <chrono>
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

#ifdef _OPENMP
#include <omp.h>
#endif

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

// g_CRC8(D) = D^8 + D^2 + D + 1 (полином 0x07, CRC-8/CCITT, SMBus).
const std::vector<uint8_t> kCrc8Polynomial = {
    1, 0, 0, 0, 0, 0, 1, 1, 1};

// g_CRC3(D) = D^3 + D + 1 (полином 0x3, CRC-3/GSM).
const std::vector<uint8_t> kCrc3Polynomial = {1, 0, 1, 1};

bool CrcPolynomialFor(const std::string& preset, std::vector<uint8_t>* out) {
  if (preset == "24a") {
    *out = kCrc24aPolynomial;
  } else if (preset == "16") {
    *out = kCrc16Polynomial;
  } else if (preset == "8") {
    *out = kCrc8Polynomial;
  } else if (preset == "3") {
    *out = kCrc3Polynomial;
  } else {
    return false;
  }
  return true;
}

struct Options {
  std::size_t blocks = 200000;
  uint32_t seed = 5489u;
  std::size_t info_bits = 512;
  std::vector<std::string> crc_presets = {"3", "8", "16", "24a"};
  std::string mode = "both";  // uncoded|coded|both
  int cc_constraint_length = 7;
  std::vector<unsigned> cc_generators = {0133u, 0171u, 0165u};  // rate 1/3
  // 0 — auto (flat fading: block_size = длина фрейма каждого режима),
  // 1 — fast fading, >1 — фиксированный размер блока когерентности.
  int block_size = 0;
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
      << " [--crc <list, e.g. 3,8,16,24a>] [--mode <uncoded|coded|both>]"
      << " [--cc-gens <octal list, e.g. 133,171,165>] [--cc-K <constraint>]"
      << " [--block-size <symbols|auto>]"
      << " [--snr <dB1,dB2,...>]"
      << " [--snr-start <dB> --snr-end <dB> --snr-points <n>]\n"
      << "\nLong-format CSV: одна строка на (crc, mode, snr).\n"
      << "Метрика P_undetected: CRC прошёл, но информационная часть искажена.\n"
      << "Коды́рование — нативный несистематический свёрточный код (по\n"
      << "умолчанию 1/3, (133,171,165)_8, K=7) с декодером Витерби.\n";
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

bool ParseStringList(const std::string& value, std::vector<std::string>* out) {
  std::vector<std::string> parsed;
  std::stringstream ss(value);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) return false;
    parsed.push_back(token);
  }
  if (parsed.empty()) return false;
  *out = std::move(parsed);
  return true;
}

bool ParseOctalList(const std::string& value, std::vector<unsigned>* out) {
  std::vector<unsigned> parsed;
  std::stringstream ss(value);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) return false;
    parsed.push_back(static_cast<unsigned>(std::stoul(token, nullptr, 8)));
  }
  if (parsed.size() < 2) return false;
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
      if (!ParseStringList(argv[++i], &options->crc_presets)) return false;
      for (const auto& p : options->crc_presets) {
        std::vector<uint8_t> dummy;
        if (!CrcPolynomialFor(p, &dummy)) {
          std::cerr << "Unsupported --crc value: " << p << " (use 8|16|24a)\n";
          return false;
        }
      }
    } else if (arg == "--mode" && i + 1 < argc) {
      options->mode = argv[++i];
      if (options->mode != "uncoded" && options->mode != "coded" &&
          options->mode != "both") {
        std::cerr << "Invalid --mode value.\n";
        return false;
      }
    } else if (arg == "--cc-gens" && i + 1 < argc) {
      if (!ParseOctalList(argv[++i], &options->cc_generators)) {
        std::cerr << "Invalid --cc-gens (need >= 2 octal values).\n";
        return false;
      }
    } else if (arg == "--cc-K" && i + 1 < argc) {
      options->cc_constraint_length = std::stoi(argv[++i]);
    } else if (arg == "--block-size" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (value == "auto") {
        options->block_size = 0;
      } else {
        options->block_size = std::stoi(value);
      }
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

// Одна расчётная единица: пара (CRC-пресет, SNR).
struct Job {
  std::size_t crc_idx;
  double snr_db;
};

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
  if (options.block_size < 0) {
    std::cerr << "block-size must be >= 1 or 'auto'.\n";
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

  const std::size_t k = options.info_bits;

  // Предрасчёт по каждому CRC: полином, r, n_crc, n_coded.
  struct CrcInfo {
    std::string preset;
    std::vector<uint8_t> polynomial;
    int r = 0;
    std::size_t n_crc = 0;
    int n_coded = 0;
  };
  std::vector<CrcInfo> crc_infos;
  for (const auto& preset : options.crc_presets) {
    CrcInfo info;
    info.preset = preset;
    CrcPolynomialFor(preset, &info.polynomial);
    harq::Crc crc(info.polynomial);
    info.r = crc.r();
    info.n_crc = k + static_cast<std::size_t>(info.r);

    if (need_coded) {
      harq::fec::FecConfig cfg{};
      cfg.codec_type = harq::fec::CodecType::kConvolutionalCc;
      cfg.cc_input_bits_per_frame = static_cast<int>(info.n_crc);
      cfg.cc_constraint_length = options.cc_constraint_length;
      cfg.cc_generators_octal = options.cc_generators;
      auto probe = harq::fec::CreateCodec(cfg);
      info.n_coded = probe->output_bits_per_frame();
    }
    crc_infos.push_back(std::move(info));
  }

  // Шапка прогона в stderr.
  std::ostringstream gens;
  for (std::size_t i = 0; i < options.cc_generators.size(); ++i) {
    if (i) gens << ",";
    gens << std::oct << options.cc_generators[i] << std::dec;
  }
  std::cerr << "CRC presets:";
  for (const auto& info : crc_infos) {
    std::cerr << " " << info.preset << "(r=" << info.r
              << ",n_crc=" << info.n_crc;
    if (need_coded) std::cerr << ",n_cod=" << info.n_coded;
    std::cerr << ")";
  }
  std::cerr << "\n";
  if (need_coded) {
    std::cerr << "Conv (native CC): rate=1/" << options.cc_generators.size()
              << ", K=" << options.cc_constraint_length
              << ", gens(oct)=" << gens.str() << ", decoder=viterbi(soft)\n";
  }
  std::cerr << "Channel: BPSK + Rayleigh, block_size=";
  if (options.block_size == 0) {
    std::cerr << "auto(flat per frame)";
  } else {
    std::cerr << options.block_size;
  }
  std::cerr << ", k=" << k << ", blocks/point=" << options.blocks;
#ifdef _OPENMP
  std::cerr << " | OpenMP threads=" << omp_get_max_threads();
#endif
  std::cerr << "\n";

  // CSV (long format).
  std::cout << "crc,r,mode,snr_db,p_undetected,p_detected,p_correct,"
               "n_undetected,n_detected,n_correct,total_blocks\n";
  std::cout.flush();

  // Список заданий: декартово произведение (CRC × SNR).
  std::vector<Job> jobs;
  for (std::size_t c = 0; c < crc_infos.size(); ++c) {
    for (double snr : snr_values) {
      jobs.push_back(Job{c, snr});
    }
  }
  const int n_jobs = static_cast<int>(jobs.size());

  // Каждый поток получает свой codec, RNG, канал и буферы.
#pragma omp parallel for schedule(dynamic, 1)
  for (int jx = 0; jx < n_jobs; ++jx) {
    const Job job = jobs[static_cast<std::size_t>(jx)];
    const CrcInfo& ci = crc_infos[job.crc_idx];
    const double snr_db = job.snr_db;
    if (!std::isfinite(snr_db)) {
#pragma omp critical(crc_undetected_log)
      std::cerr << "Invalid SNR value: " << snr_db << "\n";
      continue;
    }

    harq::Crc crc(ci.polynomial);

    std::unique_ptr<harq::fec::IFecCodec> local_codec;
    if (need_coded) {
      harq::fec::FecConfig cfg{};
      cfg.codec_type = harq::fec::CodecType::kConvolutionalCc;
      cfg.cc_input_bits_per_frame = static_cast<int>(ci.n_crc);
      cfg.cc_constraint_length = options.cc_constraint_length;
      cfg.cc_generators_octal = options.cc_generators;
      local_codec = harq::fec::CreateCodec(cfg);
    }

    harq::BpskModulator modulator;
    harq::BpskDemodulator demodulator;
    const int unc_block = options.block_size == 0
                              ? static_cast<int>(ci.n_crc)
                              : options.block_size;
    const int cod_block = options.block_size == 0
                              ? ci.n_coded
                              : options.block_size;
    harq::RayleighChannel channel(
        snr_db,
        static_cast<uint32_t>(options.seed + static_cast<uint32_t>(jx) + 1u),
        unc_block > 0 ? unc_block : 1);
    std::mt19937 rng(options.seed +
                     static_cast<uint32_t>(jx) * 0x9E3779B9u);
    std::uniform_int_distribution<int> bit_dist(0, 1);

    std::size_t und_unc = 0, det_unc = 0, ok_unc = 0;
    std::size_t und_cod = 0, det_cod = 0, ok_cod = 0;

    const auto t_start = std::chrono::steady_clock::now();

    for (std::size_t b = 0; b < options.blocks; ++b) {
      std::vector<uint8_t> message(k, 0);
      for (std::size_t i = 0; i < k; ++i) {
        message[i] = static_cast<uint8_t>(bit_dist(rng));
      }
      std::vector<uint8_t> crc_codeword = crc.Encode(message);

      if (need_uncoded) {
        if (channel.block_size() != unc_block) {
          channel.SetBlockSize(unc_block);
        }
        std::vector<double> symbols = modulator.Modulate(crc_codeword);
        std::vector<double> received = channel.AddNoise(symbols);
        std::vector<uint8_t> hard = demodulator.Demodulate(received);
        BlockOutcome o = ClassifyBlock(crc, message, hard);
        if (o.undetected) ++und_unc;
        else if (o.detected) ++det_unc;
        else ++ok_unc;
      }

      if (need_coded) {
        if (channel.block_size() != cod_block) {
          channel.SetBlockSize(cod_block);
        }
        std::vector<uint8_t> conv_encoded = local_codec->Encode(crc_codeword);
        std::vector<double> symbols = modulator.Modulate(conv_encoded);
        std::vector<double> received = channel.AddNoise(symbols);
        std::vector<double> llr =
            channel.ComputeLlr(received, channel.last_fading_amplitudes());
        std::vector<uint8_t> decoded_codeword = local_codec->DecodeSoft(llr);
        BlockOutcome o = ClassifyBlock(crc, message, decoded_codeword);
        if (o.undetected) ++und_cod;
        else if (o.detected) ++det_cod;
        else ++ok_cod;
      }
    }

    const double total = static_cast<double>(options.blocks);
    auto format_row = [&](const char* mode_name, std::size_t und,
                          std::size_t det, std::size_t ok) {
      std::ostringstream row;
      row << ci.preset << "," << ci.r << "," << mode_name << ","
          << std::setprecision(10) << std::scientific << snr_db << ","
          << und / total << "," << det / total << "," << ok / total << ","
          << und << "," << det << "," << ok << "," << options.blocks << "\n";
      return row.str();
    };

    std::string out_rows;
    if (need_uncoded) out_rows += format_row("uncoded", und_unc, det_unc, ok_unc);
    if (need_coded) out_rows += format_row("coded", und_cod, det_cod, ok_cod);

    const auto t_end = std::chrono::steady_clock::now();
    const double secs = std::chrono::duration<double>(t_end - t_start).count();

#pragma omp critical(crc_undetected_log)
    {
      std::cerr << "CRC " << ci.preset << " @ " << std::fixed
                << std::setprecision(1) << snr_db << " dB done in " << secs
                << "s";
      if (need_uncoded)
        std::cerr << " | unc und=" << und_unc << " det=" << det_unc;
      if (need_coded)
        std::cerr << " | cod und=" << und_cod << " det=" << det_cod;
      std::cerr << "\n";
      // Стримим сразу, чтобы при Ctrl+C не потерять посчитанное.
      std::cout << out_rows;
      std::cout.flush();
    }
  }

  return 0;
}
