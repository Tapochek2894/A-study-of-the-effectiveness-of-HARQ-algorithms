#include "awgn_channel.hpp"
#include "chase_combining.hpp"
#include "bpsk.hpp"
#include "fec/fec_factory.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#include <memory>
#include <random>

const int N = 5000;           // ↑ число испытаний (для BER нужна большая статистика)
const int MaximumAttempts = 10;
const uint32_t seed = 53u;

// Параметры свёрточного кода
const int CONV_K = 64;
const int CONV_RATE_NUM = 1;
const int CONV_RATE_DEN = 2;
const int CONV_DFREE = 10;

const std::vector<double> snr_values = {
    -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5
};

// Генератор случайных бит (один на программу)
std::mt19937 rng(seed);
std::uniform_int_distribution<int> bit_dist(0, 1);

// Возвращает BER (Bit Error Rate) для заданных параметров
double simulate_ber(double snr_db, harq::ProbeAlgorithm algo, bool combining) {
    // Создаём кодек один раз
    harq::fec::FecConfig cfg{};
    cfg.codec_type = harq::fec::CodecType::kConvolutionalAff3ct;
    cfg.conv_input_bits_per_frame = static_cast<std::size_t>(CONV_K);
    cfg.conv_rate_num = CONV_RATE_NUM;
    cfg.conv_rate_den = CONV_RATE_DEN;
    cfg.conv_decoder = harq::fec::ConvDecoderType::kViterbi;

    auto codec = harq::fec::CreateCodec(cfg);
    if (!codec) {
        std::cerr << "Error: Failed to create codec at SNR " << snr_db << "\n";
        return 1.0;  // худший случай
    }

    const std::size_t input_size = codec->input_bits_per_frame();
    std::size_t total_errors = 0;
    std::size_t total_bits = 0;

    for (int trial = 0; trial < N; ++trial) {
        // !!! Важно: новые случайные биты для каждого испытания
        std::vector<uint8_t> info_word(input_size);
        for (auto& b : info_word) b = static_cast<uint8_t>(bit_dist(rng));

        auto coded_bits = codec->Encode(info_word);
        auto modulated = harq::BpskModulate(coded_bits);

        std::vector<std::vector<double>> soft_history;
        std::vector<uint8_t> final_decision(input_size);

        for (int attempt = 0; attempt < MaximumAttempts; ++attempt) {
            harq::AwgnChannel channel(snr_db,
                static_cast<uint32_t>(seed + trial * MaximumAttempts + attempt * 137));
            auto soft_bits = channel.AddNoise(modulated);

            if (combining) {
                soft_history.push_back(soft_bits);
                // Агрегируем мягкие метрики (простое суммирование)
                std::vector<double> combined(soft_bits.size(), 0.0);
                for (const auto& frame : soft_history)
                    for (std::size_t i = 0; i < soft_bits.size(); ++i)
                        combined[i] += frame[i];
                final_decision = harq::DecodeConvCodesWithChase(
                    combined, algo, codec, CONV_DFREE);
            } else {
                final_decision = harq::DecodeConvCodesWithChase(
                    soft_bits, algo, codec, CONV_DFREE);
            }

            // Считаем ошибки в текущем декодировании
            std::size_t errors = 0;
            for (std::size_t i = 0; i < input_size; ++i)
                if (final_decision[i] != info_word[i]) ++errors;

            total_errors += errors;
            total_bits += input_size;

            // Если ошибок нет — успех, можно прекратить попытки для этого пакета
            if (errors == 0) break;
        }
    }

    return static_cast<double>(total_errors) / static_cast<double>(total_bits);
}

int main() {
    std::cout << std::fixed << std::setprecision(8);  // ↑ точность для малых BER
    std::cout << "snr_db,"
              << "chase2_combining,chase3_combining,"
              << "chase2_no_comb,chase3_no_comb"
              << std::endl;

    for (auto snr_db : snr_values) {
        double c2  = simulate_ber(snr_db, harq::ProbeAlgorithm::Second, true);
        double c3  = simulate_ber(snr_db, harq::ProbeAlgorithm::Third,  true);
        double nc2 = simulate_ber(snr_db, harq::ProbeAlgorithm::Second, false);
        double nc3 = simulate_ber(snr_db, harq::ProbeAlgorithm::Third,  false);

        std::cout << snr_db << ","
                  << c2 << "," << c3 << ","
                  << nc2 << "," << nc3
                  << std::endl;

        std::cerr << "SNR " << snr_db << " dB done. BER: c2=" << c2 
                  << ", nc2=" << nc2 << "\n";
    }

    return 0;
}