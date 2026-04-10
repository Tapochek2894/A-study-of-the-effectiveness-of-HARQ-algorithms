// arq_chase_sim.cpp — симуляция ARQ с алгоритмом Чейза и CRC.
//
// Схема передачи одного блока:
//   1. k_info бит → CRC-кодирование → k_hamming = k_info + r_crc бит
//   2. Хэмминг(n, k_hamming) кодирование → n бит
//   3. BPSK-модуляция → AWGN-канал → мягкое демодулирование
//   4. Chase-декодирование → k_hamming бит
//   5. Проверка CRC: если ошибка обнаружена → NACK → повторная передача
//   6. После ACK или исчерпания попыток → подсчёт метрик
//
// Режим --perfect: вместо CRC — оракульный детектор (знаем оригинал).

#include "awgn_channel.hpp"
#include "bpsk.hpp"
#include "chase_algorithm.hpp"
#include "crc.hpp"
#include "hamming_decoder.hpp"
#include "hamming_encoder.hpp"
#include "chase_combining.hpp"

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
  int r = 4;  // Параметр кода Хэмминга (2^r-1, 2^r-1-r)
  std::vector<uint8_t> crc_poly = {};  // пусто → режим perfect detection
  bool perfect_detection = false;
  int max_retx = 10;
  std::size_t blocks = 50000;
  uint32_t seed = 5489u;
  int chase_algo = 2;  // 1, 2 или 3
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
      << "  --r <n>                  Hamming code parameter r (default: 4 → (15,11))\n"
      << "  --crc-poly <1,0,1,1>    CRC polynomial coefficients\n"
      << "  --perfect                Perfect error detection (oracle, no CRC)\n"
      << "  --max-retx <n>           Max retransmissions per block (default: 10)\n"
      << "  --blocks <n>             Number of blocks (default: 50000)\n"
      << "  --seed <n>               Random seed (default: 5489)\n"
      << "  --chase-algo <1|2|3>    Chase probe algorithm (default: 2)\n"
      << "  --snr <dB,dB,...>        SNR list\n"
      << "  --snr-start <dB>         SNR range start\n"
      << "  --snr-end <dB>           SNR range end\n"
      << "  --snr-points <n>         SNR range points\n";
}

bool ParseSnrList(const std::string& s, std::vector<double>* out) {
  std::stringstream ss(s);
  std::string tok;
  out->clear();
  while (std::getline(ss, tok, ',')) {
    if (tok.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(tok.c_str(), &end);
    if (end == tok.c_str()) return false;
    out->push_back(v);
  }
  return !out->empty();
}

bool ParsePoly(const std::string& s, std::vector<uint8_t>* out) {
  std::stringstream ss(s);
  std::string tok;
  out->clear();
  while (std::getline(ss, tok, ',')) {
    if (tok.empty()) return false;
    int c = std::stoi(tok);
    if (c != 0 && c != 1) return false;
    out->push_back(static_cast<uint8_t>(c));
  }
  if (out->size() < 2 || out->front() != 1) return false;
  return true;
}

bool ParseArgs(int argc, char** argv, Options* opt) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--r" && i + 1 < argc) {
      opt->r = std::stoi(argv[++i]);
    } else if (a == "--crc-poly" && i + 1 < argc) {
      if (!ParsePoly(argv[++i], &opt->crc_poly)) return false;
    } else if (a == "--perfect") {
      opt->perfect_detection = true;
    } else if (a == "--max-retx" && i + 1 < argc) {
      opt->max_retx = std::stoi(argv[++i]);
    } else if (a == "--blocks" && i + 1 < argc) {
      opt->blocks = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (a == "--seed" && i + 1 < argc) {
      opt->seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (a == "--chase-algo" && i + 1 < argc) {
      opt->chase_algo = std::stoi(argv[++i]);
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
      std::cerr << "Unknown argument: " << a << "\n";
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  if (!ParseArgs(argc, argv, &opt)) {
    PrintUsage(argv[0]);
    return 1;
  }

  if (opt.perfect_detection && !opt.crc_poly.empty()) {
    std::cerr << "Error: --perfect and --crc-poly are mutually exclusive.\n";
    return 1;
  }
  if (!opt.perfect_detection && opt.crc_poly.empty()) {
    std::cerr << "Error: specify --crc-poly or --perfect.\n";
    return 1;
  }
  if (opt.r < 2) {
    std::cerr << "Error: r must be >= 2.\n";
    return 1;
  }
  if (opt.max_retx < 0) {
    std::cerr << "Error: max-retx must be >= 0.\n";
    return 1;
  }
  if (opt.chase_algo < 1 || opt.chase_algo > 3) {
    std::cerr << "Error: chase-algo must be 1, 2, or 3.\n";
    return 1;
  }

  // Параметры кода
  const int n = (1 << opt.r) - 1;      // длина кодового слова Хэмминга
  const int k_hamming = n - opt.r;     // размерность кода Хэмминга

  // Параметры CRC
  int r_crc = 0;
  std::unique_ptr<harq::Crc> crc;
  if (!opt.perfect_detection) {
    crc = std::make_unique<harq::Crc>(opt.crc_poly);
    r_crc = crc->r();
  }

  const int k_info = k_hamming - r_crc;  // информационных бит на блок
  if (k_info <= 0) {
    std::cerr << "Error: CRC degree (" << r_crc
              << ") is too large for Hamming(" << n << "," << k_hamming << ").\n";
    return 1;
  }

  harq::HammingEncoder encoder(opt.r);
  harq::HammingDecoder decoder(opt.r);

  harq::ProbeAlgorithm chase_algo;
  switch (opt.chase_algo) {
    case 1: chase_algo = harq::ProbeAlgorithm::First; break;
    case 2: chase_algo = harq::ProbeAlgorithm::Second; break;
    default: chase_algo = harq::ProbeAlgorithm::Third; break;
  }

  // Информация о конфигурации в stderr
  std::cerr << "HARQ Chase Simulation\n";
  std::cerr << "  Hamming(" << n << "," << k_hamming << "), r=" << opt.r << "\n";
  if (opt.perfect_detection) {
    std::cerr << "  Detection: perfect (oracle)\n";
  } else {
    std::cerr << "  CRC degree: " << r_crc << ", polynomial: ";
    for (std::size_t i = 0; i < opt.crc_poly.size(); ++i) {
      std::cerr << static_cast<int>(opt.crc_poly[i]);
      if (i + 1 < opt.crc_poly.size()) std::cerr << ",";
    }
    std::cerr << "\n";
  }
  std::cerr << "  k_info=" << k_info << ", max_retx=" << opt.max_retx
            << ", blocks=" << opt.blocks << "\n\n";

  // Построение сетки SNR
  std::vector<double> snr_values = opt.snr_list;
  if (opt.use_range) {
    snr_values.clear();
    for (int i = 0; i < opt.snr_points; ++i) {
      double t = static_cast<double>(i) / static_cast<double>(opt.snr_points - 1);
      snr_values.push_back(opt.snr_start + t * (opt.snr_end - opt.snr_start));
    }
  }

  std::mt19937 rng(opt.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);

  // CSV заголовок
  std::cout << "snr_db,ber,bler,avg_retx,"
            << "bit_errors,block_errors,total_retx,total_blocks\n";
  std::cout << std::setprecision(8) << std::fixed;

  for (std::size_t snr_idx = 0; snr_idx < snr_values.size(); ++snr_idx) {
    const double snr_db = snr_values[snr_idx];

    std::size_t total_bit_errors = 0;
    std::size_t total_block_errors = 0;
    std::size_t total_retx = 0;  // суммарное число попыток по всем блокам

    for (std::size_t b = 0; b < opt.blocks; ++b) {
      // 1. Генерируем k_info случайных информационных бит
      std::vector<uint8_t> info(k_info);
      for (int i = 0; i < k_info; ++i) {
        info[i] = static_cast<uint8_t>(bit_dist(rng));
      }
      std::vector<double> soft_decisions;
      // 2. CRC-кодирование: [info | crc_bits] = k_hamming бит
      std::vector<uint8_t> hamming_input;
      if (opt.perfect_detection) {
        hamming_input = info;
      } else {
        hamming_input = crc->Encode(info);  // k_info + r_crc = k_hamming
      }

      // 3. Хэмминг-кодирование → n бит
      std::vector<uint8_t> codeword = encoder.Encode(hamming_input);

      // 4. BPSK-модуляция
      std::vector<double> symbols = harq::BpskModulate(codeword);

      // ARQ-цикл
      std::vector<uint8_t> decoded_info(k_info, 0);
      int attempts = 0;

      for (int attempt = 0; attempt <= opt.max_retx; ++attempt) {
        ++attempts;

        // Канал: разный сид для каждой попытки (независимый шум)
        const uint32_t ch_seed = opt.seed +
            static_cast<uint32_t>(snr_idx) * 1000003u +
            static_cast<uint32_t>(b) * 1009u +
            static_cast<uint32_t>(attempt) * 101u;
        harq::AwgnChannel channel(snr_db, ch_seed);

        auto [received, llr] = channel.Transmit(symbols);
        (void)llr;  // используем raw received для Chase (BPSK: symbol = ±1)

        // Chase-декодирование → k_hamming бит
        std::vector<uint8_t> decoded_block =
            harq::DecodeHammingCodesWithChase(received, chase_algo, decoder);

        // Извлекаем информационные биты (первые k_info)
        decoded_info.assign(decoded_block.begin(),
                            decoded_block.begin() + k_info);

        // 5. Детектирование ошибки
        bool error_detected = false;
        if (opt.perfect_detection) {
          // Оракул: знаем оригинал
          for (int i = 0; i < k_info; ++i) {
            if (decoded_info[i] != info[i]) {
              error_detected = true;
              break;
            }
          }
        } else {
          // CRC-проверка: вычисляем CRC по декодированным info-битам
          // и сравниваем с CRC-битами из decoded_block
          std::vector<uint8_t> crc_bits_decoded(
              decoded_block.begin() + k_info, decoded_block.end());
          std::vector<uint8_t> crc_bits_expected = crc->Encode(decoded_info);
          // Encode возвращает [info | crc], берём хвост
          std::vector<uint8_t> crc_expected_tail(
              crc_bits_expected.begin() + k_info, crc_bits_expected.end());
          error_detected = (crc_bits_decoded != crc_expected_tail);
        }

        if (!error_detected || attempt == opt.max_retx) {
          break;  // ACK или исчерпали попытки
        }
        // NACK → повторная передача (тот же codeword, новый шум)
      }

      total_retx += static_cast<std::size_t>(attempts);

      // Подсчёт ошибок в финальном результате
      bool block_has_error = false;
      for (int i = 0; i < k_info; ++i) {
        if (decoded_info[i] != info[i]) {
          ++total_bit_errors;
          block_has_error = true;
        }
      }
      if (block_has_error) ++total_block_errors;
    }

    const double total_info_bits =
        static_cast<double>(opt.blocks) * static_cast<double>(k_info);
    const double ber = static_cast<double>(total_bit_errors) / total_info_bits;
    const double bler =
        static_cast<double>(total_block_errors) / static_cast<double>(opt.blocks);
    const double avg_retx =
        static_cast<double>(total_retx) / static_cast<double>(opt.blocks);

    std::cout << snr_db << "," << ber << "," << bler << "," << avg_retx << ","
              << total_bit_errors << "," << total_block_errors << ","
              << total_retx << "," << opt.blocks << "\n";
  }

  return 0;
}
