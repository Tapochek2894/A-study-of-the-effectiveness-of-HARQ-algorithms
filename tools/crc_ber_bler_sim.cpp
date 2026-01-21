#include "crc.hpp"

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
  std::vector<uint8_t> polynomial = {1, 0, 1, 1};  // x³ + x + 1 по умолчанию
  std::size_t message_len = 8;  // Длина информационного сообщения
  std::size_t blocks = 10000;
  uint32_t seed = 5489u;
  std::vector<double> p_list = {1e-4, 5e-4, 1e-3, 5e-3, 1e-2};
  bool use_range = false;
  double p_start = 1e-4;
  double p_end = 1e-2;
  int p_points = 9;
  bool p_log = true;
};

// Результат для одного паттерна ошибок
struct ErrorOutcome {
  uint8_t weight = 0;       // Вес ошибки (количество искажённых бит)
  bool detected = false;    // Ошибка обнаружена (синдром != 0)
  uint8_t info_errors = 0;  // Количество ошибок в информационной части
};

// Предвычисление результатов для всех возможных паттернов ошибок
std::vector<ErrorOutcome> PrecomputeErrorOutcomes(const harq::Crc& crc,
                                                   std::size_t n) {
  const int r = crc.r();
  const std::size_t k = n - static_cast<std::size_t>(r);

  if (n > 20) {
    // Для больших n полный перебор невозможен
    return {};
  }

  std::vector<ErrorOutcome> outcomes;
  const std::uint32_t total = 1u << n;
  outcomes.reserve(total);

  std::vector<uint8_t> error_vec(n, 0);
  for (std::uint32_t mask = 0; mask < total; ++mask) {
    for (std::size_t bit = 0; bit < n; ++bit) {
      error_vec[bit] = static_cast<uint8_t>((mask >> bit) & 1u);
    }

    // Проверяем, обнаруживается ли ошибка
    bool detected = crc.HasError(error_vec);

    // Подсчитываем ошибки в информационной части
    int info_errors = 0;
    for (std::size_t i = 0; i < k; ++i) {
      if (error_vec[i] == 1) {
        ++info_errors;
      }
    }

    ErrorOutcome outcome;
    outcome.weight = static_cast<uint8_t>(__builtin_popcount(mask));
    outcome.detected = detected;
    outcome.info_errors = static_cast<uint8_t>(info_errors);
    outcomes.push_back(outcome);
  }

  return outcomes;
}

void PrintUsage(const char* argv0) {
  std::cout
      << "Usage: " << argv0
      << " [--poly <1,0,1,1>] [--msg-len <length>] [--blocks <count>]"
      << " [--seed <seed>] [--p <p1,p2,...>]"
      << " [--p-start <value> --p-end <value> --p-points <n> [--p-linear]]\n"
      << "\nOptions:\n"
      << "  --poly      CRC polynomial coefficients (default: 1,0,1,1 = x^3+x+1)\n"
      << "  --msg-len   Message length in bits (default: 8)\n"
      << "  --blocks    Number of blocks to simulate (default: 10000)\n"
      << "  --seed      Random seed (default: 5489)\n"
      << "  --p         Comma-separated list of error probabilities\n"
      << "  --p-start   Start of probability range\n"
      << "  --p-end     End of probability range\n"
      << "  --p-points  Number of points in range\n"
      << "  --p-linear  Use linear scale (default: logarithmic)\n";
}

bool ParseList(const std::string& value, std::vector<double>* out) {
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

bool ParsePolynomial(const std::string& value, std::vector<uint8_t>* out) {
  std::vector<uint8_t> parsed;
  std::stringstream ss(value);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) {
      return false;
    }
    int coef = std::stoi(token);
    if (coef != 0 && coef != 1) {
      return false;
    }
    parsed.push_back(static_cast<uint8_t>(coef));
  }
  if (parsed.size() < 2 || parsed[0] != 1) {
    return false;
  }
  *out = std::move(parsed);
  return true;
}

bool ParseArgs(int argc, char** argv, Options* options) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--poly" && i + 1 < argc) {
      if (!ParsePolynomial(argv[++i], &options->polynomial)) {
        std::cerr << "Invalid polynomial format.\n";
        return false;
      }
    } else if (arg == "--msg-len" && i + 1 < argc) {
      options->message_len = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (arg == "--blocks" && i + 1 < argc) {
      options->blocks = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      options->seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--p" && i + 1 < argc) {
      if (!ParseList(argv[++i], &options->p_list)) {
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
      std::cerr << "Unknown argument: " << arg << "\n";
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
  if (options.message_len == 0) {
    std::cerr << "Message length must be positive.\n";
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

  harq::Crc crc(options.polynomial);
  const int r = crc.r();
  const std::size_t k = options.message_len;
  const std::size_t n = k + static_cast<std::size_t>(r);

  // Предвычисление для теоретических значений
  std::vector<ErrorOutcome> error_outcomes = PrecomputeErrorOutcomes(crc, n);
  const bool has_theory = !error_outcomes.empty();

  std::mt19937 rng(options.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);
  std::uniform_real_distribution<double> uni(0.0, 1.0);

  // Вывод информации о CRC
  std::cerr << "CRC parameters:\n";
  std::cerr << "  Polynomial: ";
  for (std::size_t i = 0; i < options.polynomial.size(); ++i) {
    std::cerr << static_cast<int>(options.polynomial[i]);
    if (i < options.polynomial.size() - 1) std::cerr << ",";
  }
  std::cerr << " (degree r=" << r << ")\n";
  std::cerr << "  Message length k=" << k << ", Codeword length n=" << n << "\n";
  std::cerr << "  Blocks: " << options.blocks << "\n";
  if (has_theory) {
    std::cerr << "  Theoretical values: enabled (exhaustive enumeration)\n";
  } else {
    std::cerr << "  Theoretical values: approximation (n > 20)\n";
  }
  std::cerr << "\n";

  // CSV заголовок
  // BER - bit error rate после "декодирования" (CRC не исправляет, так что = channel BER)
  // BLER - block error rate (реальные ошибки в блоке)
  // P_ud - probability of undetected error (ошибки есть, но синдром = 0)
  // P_d - probability of detected error (синдром != 0)
  std::cout << "p,ber,bler,p_detected,p_undetected,"
            << "ber_theory,bler_theory,p_detected_theory,p_undetected_theory,"
            << "bit_errors,block_errors,detected_errors,undetected_errors,"
            << "total_bits,total_blocks\n";
  std::cout << std::setprecision(10) << std::scientific;

  std::vector<double> p_values = options.p_list;
  if (options.use_range) {
    p_values.clear();
    if (options.p_log) {
      const double log_start = std::log10(options.p_start);
      const double log_end = std::log10(options.p_end);
      for (int i = 0; i < options.p_points; ++i) {
        double t =
            static_cast<double>(i) / static_cast<double>(options.p_points - 1);
        double value = std::pow(10.0, log_start + t * (log_end - log_start));
        p_values.push_back(value);
      }
    } else {
      for (int i = 0; i < options.p_points; ++i) {
        double t =
            static_cast<double>(i) / static_cast<double>(options.p_points - 1);
        double value = options.p_start + t * (options.p_end - options.p_start);
        p_values.push_back(value);
      }
    }
  }

  for (double p : p_values) {
    // Теоретические значения
    long double ber_theory = 0.0L;
    long double bler_theory = 0.0L;
    long double p_detected_theory = 0.0L;
    long double p_undetected_theory = 0.0L;

    if (has_theory) {
      const long double p_ld = static_cast<long double>(p);
      const long double q_ld = 1.0L - p_ld;

      for (const auto& outcome : error_outcomes) {
        const int w = outcome.weight;
        const long double prob =
            std::pow(p_ld, w) * std::pow(q_ld, static_cast<int>(n) - w);

        // BER: среднее количество ошибок в информационной части
        ber_theory += static_cast<long double>(outcome.info_errors) * prob;

        if (w > 0) {  // Есть ошибки в кодовом слове
          bler_theory += prob;
          if (outcome.detected) {
            p_detected_theory += prob;
          } else {
            p_undetected_theory += prob;
          }
        }
      }
      ber_theory /= static_cast<long double>(k);
    } else {
      // Приближённые формулы для больших n
      // BER ≈ p (CRC не исправляет ошибки)
      ber_theory = static_cast<long double>(p);
      // BLER ≈ 1 - (1-p)^n
      bler_theory = 1.0L - std::pow(1.0L - static_cast<long double>(p),
                                     static_cast<long double>(n));
      // P_ud ≈ 2^(-r) * BLER (приближение для случайных ошибок)
      p_undetected_theory =
          bler_theory * std::pow(2.0L, -static_cast<long double>(r));
      p_detected_theory = bler_theory - p_undetected_theory;
    }

    // Симуляция
    std::size_t bit_errors = 0;
    std::size_t block_errors = 0;
    std::size_t detected_errors = 0;
    std::size_t undetected_errors = 0;
    std::size_t total_bits = 0;

    for (std::size_t b = 0; b < options.blocks; ++b) {
      // Генерируем случайное сообщение
      std::vector<uint8_t> message(k, 0);
      for (std::size_t i = 0; i < k; ++i) {
        message[i] = static_cast<uint8_t>(bit_dist(rng));
      }

      // Кодируем
      std::vector<uint8_t> codeword = crc.Encode(message);

      // Вносим ошибки в канале
      bool has_channel_error = false;
      for (uint8_t& bit : codeword) {
        if (uni(rng) < p) {
          bit ^= 1;
          has_channel_error = true;
        }
      }

      // Проверяем CRC
      bool error_detected = crc.HasError(codeword);

      // Декодируем (извлекаем информационную часть)
      std::vector<uint8_t> decoded = crc.Decode(codeword);

      // Подсчитываем ошибки в информационной части
      bool has_info_error = false;
      for (std::size_t i = 0; i < k; ++i) {
        if (decoded[i] != message[i]) {
          ++bit_errors;
          has_info_error = true;
        }
      }

      if (has_info_error) {
        ++block_errors;
      }

      // Классификация результата
      if (has_channel_error) {
        if (error_detected) {
          ++detected_errors;
        } else {
          ++undetected_errors;
        }
      }

      total_bits += k;
    }

    double ber = static_cast<double>(bit_errors) / static_cast<double>(total_bits);
    double bler =
        static_cast<double>(block_errors) / static_cast<double>(options.blocks);
    double p_detected = static_cast<double>(detected_errors) /
                        static_cast<double>(options.blocks);
    double p_undetected = static_cast<double>(undetected_errors) /
                          static_cast<double>(options.blocks);

    std::cout << p << "," << ber << "," << bler << "," << p_detected << ","
              << p_undetected << "," << static_cast<double>(ber_theory) << ","
              << static_cast<double>(bler_theory) << ","
              << static_cast<double>(p_detected_theory) << ","
              << static_cast<double>(p_undetected_theory) << "," << bit_errors
              << "," << block_errors << "," << detected_errors << ","
              << undetected_errors << "," << total_bits << ","
              << options.blocks << "\n";
  }

  return 0;
}
