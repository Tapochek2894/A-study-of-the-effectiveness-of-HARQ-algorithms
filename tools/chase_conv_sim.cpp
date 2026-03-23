#include "awgn_channel.hpp"
#include "qpsk.hpp"
#include "chase_algorithm.hpp"
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
#include <algorithm>

namespace {

struct Options {
  std::size_t total_bits = 100000;
  uint32_t seed = 5489u;
  std::vector<double> snr_list = {-6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4};
  bool use_range = false;
  double snr_start = -6.0;
  double snr_end = 4.0;
  int snr_points = 11;
  
  int conv_k = 128;
  int conv_rate_num = 1;
  int conv_rate_den = 2;
  
  int conv_dfree = 10;
  
  bool enable_chase1 = true;
  bool enable_chase2 = true;
  bool enable_chase3 = true;
};

void PrintUsage(const char *argv0) {
  std::cout << "Usage: " << argv0 << " [options]\n\n"
            << "General:\n"
            << "  --bits <N>           Total bits (default: 100000)\n"
            << "  --seed <S>           RNG seed (default: 5489)\n"
            << "  --snr <dB1,dB2,...>  SNR list\n"
            << "  --snr-range <start> <end> <points>  SNR range\n"
            << "\nConvolutional code:\n"
            << "  --conv-k <K>         Input bits/frame (default: 64)\n"
            << "  --conv-rate <n/d>    Code rate (default: 1/2)\n"
            << "  --conv-dfree <d>     Free distance (default: auto)\n"
            << "\nChase:\n"
            << "  --no-chase1/2/3      Disable specific Chase variant\n"
            << "\nExample:\n"
            << "  " << argv0 << " --bits 50000 --conv-k 64 --conv-dfree 5 --snr 0,1,2,3\n";
}

// ============================================================================
// Парсинг
// ============================================================================
bool ParseSnrList(const std::string &value, std::vector<double> *out) {
  std::vector<double> parsed;
  std::stringstream ss(value);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (token.empty()) return false;
    char *endptr = nullptr;
    double v = std::strtod(token.c_str(), &endptr);
    if (endptr == token.c_str()) return false;
    parsed.push_back(v);
  }
  if (parsed.empty()) return false;
  *out = std::move(parsed);
  return true;
}

bool ParseArgs(int argc, char **argv, Options *options) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--bits" && i + 1 < argc) {
      options->total_bits = static_cast<std::size_t>(std::stoull(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      options->seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--snr" && i + 1 < argc) {
      if (!ParseSnrList(argv[++i], &options->snr_list)) return false;
      options->use_range = false;
    } else if (arg == "--snr-range" && i + 3 < argc) {
      options->snr_start = std::stod(argv[++i]);
      options->snr_end = std::stod(argv[++i]);
      options->snr_points = std::stoi(argv[++i]);
      options->use_range = true;
    } else if (arg == "--conv-k" && i + 1 < argc) {
      options->conv_k = std::stoi(argv[++i]);
    } else if (arg == "--conv-rate" && i + 1 < argc) {
      std::string rate = argv[++i];
      auto pos = rate.find('/');
      if (pos == std::string::npos) return false;
      options->conv_rate_num = std::stoi(rate.substr(0, pos));
      options->conv_rate_den = std::stoi(rate.substr(pos + 1));
    } else if (arg == "--conv-dfree" && i + 1 < argc) {
      options->conv_dfree = std::stoi(argv[++i]);
      if (options->conv_dfree <= 0) {
        std::cerr << "Error: conv-dfree must be positive.\n";
        return false;
      }
    } else if (arg == "--no-chase1") {
      options->enable_chase1 = false;
    } else if (arg == "--no-chase2") {
      options->enable_chase2 = false;
    } else if (arg == "--no-chase3") {
      options->enable_chase3 = false;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      return false;
    }
  }
  return true;
}

// ============================================================================
// Вычисление свободного расстояния d_free
// ============================================================================
int ComputeConvolutionalDfree(int constraint_length, 
                               int rate_num, int rate_den) {
  // Таблица известных значений d_free для распространённых кодов
  // Источник: J.G. Proakis, "Digital Communications", Table 8.2-1
  
  // Rate 1/2 коды:
  if (rate_num == 1 && rate_den == 2) {
    switch (constraint_length) {
      case 3:  return 5;   // (7,5)
      case 4:  return 6;   // (15,17)
      case 5:  return 7;   // (23,35)
      case 6:  return 8;   // (53,75)
      case 7:  return 10;  // (133,171) — стандартный код NASA/ESA
      case 8:  return 12;  // (247,371)
      case 9:  return 13;  // (561,753)
    }
  }
  
  // Rate 1/3 коды:
  if (rate_num == 1 && rate_den == 3) {
    switch (constraint_length) {
      case 3: return 8;
      case 4: return 10;
      case 5: return 12;
      case 6: return 14;
      case 7: return 18;  // (133,145,175)
    }
  }
  
  // Эвристика для неизвестных кодов:
  // d_free ≈ (K-1) * rate_den + 1 (верхняя оценка)
  return (constraint_length - 1) * rate_den + 1;
}

// Оцениваем K из размеров фрейма для систематического RSC rate-1/2
int EstimateConstraintLength(std::size_t input_bits, std::size_t output_bits) {
  // Для systematic RSC rate-1/2: output = 2*input + 2*(K-1)
  // => K = (output - 2*input) / 2 + 1
  if (output_bits <= 2 * input_bits) return 3;  // минимальное K
  int tail_bits = static_cast<int>(output_bits - 2 * input_bits);
  int K = tail_bits / 2 + 1;
  if (K < 3) K = 3;
  if (K > 9) K = 7;  // fallback к стандартному
  return K;
}

// ============================================================================
// Вспомогательные
// ============================================================================
std::size_t CountErrors(const std::vector<uint8_t>& decoded,
                        const std::vector<uint8_t>& original) {
  std::size_t err = 0;
  for (std::size_t i = 0; i < original.size(); ++i) {
    if (decoded[i] != original[i]) ++err;
  }
  return err;
}

} // namespace

// ============================================================================
// MAIN
// ============================================================================
int main(int argc, char **argv) {
  Options options;
  if (!ParseArgs(argc, argv, &options)) {
    PrintUsage(argv[0]);
    return 1;
  }

  // Валидация
  if (options.conv_k <= 0 || options.conv_rate_num <= 0 || options.conv_rate_den <= 0) {
    std::cerr << "Error: Invalid convolutional code parameters.\n";
    return 1;
  }
  if (options.conv_rate_num >= options.conv_rate_den) {
    std::cerr << "Error: Code rate must be < 1.\n";
    return 1;
  }

  // SNR список
  std::vector<double> snr_values = options.snr_list;
  if (options.use_range) {
    snr_values.clear();
    for (int i = 0; i < options.snr_points; ++i) {
      double t = static_cast<double>(i) / (options.snr_points - 1);
      snr_values.push_back(options.snr_start + t * (options.snr_end - options.snr_start));
    }
  }

  // Создание кодека (использует дефолтные полиномы библиотеки)
  harq::fec::FecConfig cfg{};
  cfg.codec_type = harq::fec::CodecType::kConvolutionalAff3ct;
  cfg.conv_input_bits_per_frame = static_cast<std::size_t>(options.conv_k);
  cfg.conv_rate_num = options.conv_rate_num;
  cfg.conv_rate_den = options.conv_rate_den;
  cfg.conv_decoder = harq::fec::ConvDecoderType::kViterbi;

  auto codec = harq::fec::CreateCodec(cfg);
  if (!codec) {
    std::cerr << "Error: Failed to create convolutional codec.\n";
    return 1;
  }

  const std::size_t input_frame_size = codec->input_bits_per_frame();
  const std::size_t output_frame_size = codec->output_bits_per_frame();

  // === ВЫЧИСЛЕНИЕ d_free ===
  int d_free = options.conv_dfree;
  if (d_free < 0) {
    // Оцениваем K из размеров фрейма
    int estimated_K = EstimateConstraintLength(input_frame_size, output_frame_size);
    d_free = ComputeConvolutionalDfree(estimated_K, 
                                        options.conv_rate_num, 
                                        options.conv_rate_den);
    std::cerr << "[Auto] Estimated K=" << estimated_K << ", d_free=" << d_free << "\n";
  }
  const int t_max = d_free / 2;  // Максимальное число исправляемых ошибок

  std::cerr << "=== Codec Configuration ===\n"
            << "  Input:       " << input_frame_size << " bits\n"
            << "  Output:      " << output_frame_size << " bits\n"
            << "  Rate:        " << options.conv_rate_num << "/" << options.conv_rate_den << "\n"
            << "  d_free:      " << d_free << "\n"
            << "  Note:        Using library default polynomials\n"
            << "===========================\n";

  // Выравнивание бит
  std::size_t aligned_bits = (options.total_bits / input_frame_size) * input_frame_size;
  if (aligned_bits == 0) {
    std::cerr << "Error: total_bits must be >= conv-k.\n";
    return 1;
  }
  if (aligned_bits != options.total_bits) {
    std::cerr << "Warning: trimmed to " << aligned_bits << " bits.\n";
  }

  // CSV заголовок
  std::cout << "snr_db,ber_uncoded,ber_viterbi";
  if (options.enable_chase1) std::cout << ",ber_chase1";
  if (options.enable_chase2) std::cout << ",ber_chase2";
  if (options.enable_chase3) std::cout << ",ber_chase3";
  std::cout << ",errors_uncoded,errors_viterbi";
  if (options.enable_chase1) std::cout << ",errors_chase1";
  if (options.enable_chase2) std::cout << ",errors_chase2";
  if (options.enable_chase3) std::cout << ",errors_chase3";
  std::cout << ",total_bits\n";
  std::cout << std::setprecision(8) << std::fixed;

  // RNG
  std::mt19937 rng(options.seed);
  std::uniform_int_distribution<int> bit_dist(0, 1);

  // Цикл по SNR
  for (std::size_t snr_idx = 0; snr_idx < snr_values.size(); ++snr_idx) {
    double snr_db = snr_values[snr_idx];
    if (!std::isfinite(snr_db)) {
      std::cerr << "Error: Invalid SNR: " << snr_db << "\n";
      return 1;
    }
    
    harq::AwgnChannel channel(snr_db, options.seed + static_cast<uint32_t>(snr_idx));

    // Генерация бит
    std::vector<uint8_t> info_bits(aligned_bits);
    for (auto &b : info_bits) b = static_cast<uint8_t>(bit_dist(rng));

    // Uncoded
    const auto uncoded_symbols = harq::QpskModulate(info_bits);
    const auto uncoded_rx = channel.TransmitComplex(uncoded_symbols);
    const auto uncoded_decoded = harq::QpskDemodulateHard(uncoded_rx, snr_db);
    const std::size_t errors_uncoded = CountErrors(uncoded_decoded, info_bits);
    const double ber_uncoded = static_cast<double>(errors_uncoded) / aligned_bits;

    // Кодирование
    std::vector<uint8_t> coded_bits;
    coded_bits.reserve(aligned_bits * options.conv_rate_den / options.conv_rate_num + 10);
    
    for (std::size_t i = 0; i < aligned_bits; i += input_frame_size) {
      std::vector<uint8_t> frame(info_bits.begin() + i, info_bits.begin() + i + input_frame_size);
      auto encoded = codec->Encode(frame);
      coded_bits.insert(coded_bits.end(), encoded.begin(), encoded.end());
    }

    // Модуляция + канал
    const auto coded_symbols = harq::QpskModulate(coded_bits);
    const auto coded_rx = channel.TransmitComplex(coded_symbols);
    
    // Демодуляция
    const auto soft_demod = harq::QpskDemodulate(coded_rx, snr_db);
    const auto hard_demod = harq::QpskDemodulateHard(coded_rx, snr_db);

    // Viterbi (soft-decision)
    std::vector<uint8_t> decoded_viterbi;
    decoded_viterbi.reserve(aligned_bits);
    
    for (std::size_t i = 0; i < coded_bits.size(); i += output_frame_size) {
      // std::vector<double> frame_soft(soft_demod.begin() + i, soft_demod.begin() + i + output_frame_size);
      // for (auto& val : frame_soft) val = -val;
      // auto dec = codec->DecodeSoft(frame_soft);

      std::vector<uint8_t> frame_hard(hard_demod.begin() + i, hard_demod.begin() + i + output_frame_size);
      auto dec = codec->DecodeHard(frame_hard);
      
      decoded_viterbi.insert(decoded_viterbi.end(), dec.begin(), dec.end());
    }
    const std::size_t errors_viterbi = CountErrors(decoded_viterbi, info_bits);
    const double ber_viterbi = static_cast<double>(errors_viterbi) / aligned_bits;

    // Chase decoding
    std::size_t errors_chase1 = 0, errors_chase2 = 0, errors_chase3 = 0;
    
    if (options.enable_chase1 || options.enable_chase2 || options.enable_chase3) {
      for (std::size_t i = 0; i < coded_bits.size(); i += output_frame_size) {
        std::vector<double> frame_soft(soft_demod.begin() + i, soft_demod.begin() + i + output_frame_size);
        std::size_t frame_idx = i / output_frame_size;
        std::size_t info_start = frame_idx * input_frame_size;
        
        if (options.enable_chase1) {
          auto dec = harq::DecodeConvCodesWithChase(
              frame_soft, harq::ProbeAlgorithm::First, codec, d_free);
          for (std::size_t j = 0; j < dec.size(); ++j) {
            if (dec[j] != info_bits[info_start + j]) ++errors_chase1;
          }
        }
        if (options.enable_chase2) {
          auto dec = harq::DecodeConvCodesWithChase(
              frame_soft, harq::ProbeAlgorithm::Second, codec, d_free);
          for (std::size_t j = 0; j < dec.size(); ++j) {
            if (dec[j] != info_bits[info_start + j]) ++errors_chase2;
          }
        }
        if (options.enable_chase3) {
          auto dec = harq::DecodeConvCodesWithChase(
              frame_soft, harq::ProbeAlgorithm::Third, codec, d_free);
          for (std::size_t j = 0; j < dec.size(); ++j) {
            if (dec[j] != info_bits[info_start + j]) ++errors_chase3;
          }
        }
      }
    }

    // Вывод
    std::cout << snr_db << "," << ber_uncoded << "," << ber_viterbi;
    if (options.enable_chase1) std::cout << "," << static_cast<double>(errors_chase1) / aligned_bits;
    if (options.enable_chase2) std::cout << "," << static_cast<double>(errors_chase2) / aligned_bits;
    if (options.enable_chase3) std::cout << "," << static_cast<double>(errors_chase3) / aligned_bits;
    std::cout << "," << errors_uncoded << "," << errors_viterbi;
    if (options.enable_chase1) std::cout << "," << errors_chase1;
    if (options.enable_chase2) std::cout << "," << errors_chase2;
    if (options.enable_chase3) std::cout << "," << errors_chase3;
    std::cout << "," << aligned_bits << "\n";
    std::cout.flush();
    
    std::cerr << "SNR " << snr_idx + 1 << "/" << snr_values.size() 
              << " (" << snr_db << " dB) done.\n";
  }

  std::cerr << "Simulation complete.\n";
  return 0;
}