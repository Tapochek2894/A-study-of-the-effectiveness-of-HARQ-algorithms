#include "awgn_channel.hpp"
#include "bpsk.hpp"
#include "chase_combining.hpp"
#include "hamming_decoder.hpp"
#include "hamming_encoder.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>
#include <iomanip>

const int N = 10000;                    // Число пакетов на точку SNR
const int r = 4;                       // Параметр кода Хэмминга: (2^r-1, 2^r-1-r)
const int MaximumAttempts = 10;        // Максимум попыток HARQ
const uint32_t seed = 593u;
const std::vector<double> snr_values = {
    -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2
};


double simulate_ber(double snr_db, harq::ProbeAlgorithm algo, bool combining) {
    harq::HammingEncoder encoder(r);
    harq::HammingDecoder decoder(r);

    const int k = (1 << r) - 1 - r;  // Информационные биты
    const int n = (1 << r) - 1;       // Длина кодового слова

    // RNG для генерации случайных данных
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> bit_dist(0, 1);

    std::size_t total_info_bits = 0;
    std::size_t total_bit_errors = 0;

    for (int pkt = 0; pkt < N; ++pkt) {
        // 1. Генерация случайного информационного слова
        std::vector<uint8_t> info_word(k);
        for (int i = 0; i < k; ++i) {
            info_word[i] = static_cast<uint8_t>(bit_dist(rng));
        }

        // 2. Кодирование и модуляция (однократно)
        auto codeword = encoder.Encode(info_word);
        auto modulated = harq::BpskModulate(codeword);

        // 3. HARQ-цикл
        std::vector<std::vector<double>> soft_history;
        std::vector<uint8_t> final_decision(k, 0);
        bool success = false;

        for (int attempt = 0; attempt < MaximumAttempts; ++attempt) {
            // Канал с уникальным seed для каждой попытки
            harq::AwgnChannel channel(snr_db,
                static_cast<uint32_t>(seed + pkt * MaximumAttempts + attempt));
            
            auto soft_bits = channel.AddNoise(modulated);

            if (combining) {
                // Chase combining: накапливаем мягкие биты
                soft_history.push_back(soft_bits);
                final_decision = harq::ChaseCombiningHammingNoCRC(algo, decoder, soft_history);
            } else {
                // Независимое декодирование каждой попытки
                final_decision = harq::DecodeHammingCodesWithChase(soft_bits, algo, decoder);
            }

            // Проверка на успех (сравнение с оригиналом)
            bool packet_ok = true;
            for (int i = 0; i < k; ++i) {
                if (final_decision[i] != info_word[i]) {
                    packet_ok = false;
                    break;
                }
            }

            if (packet_ok) {
                success = true;
                break;  // Успех — выходим из цикла HARQ
            }
            // Если ошибка — продолжаем попытки (до MaximumAttempts)
        }

        // 4. Подсчёт битовых ошибок
        // Если успех после любой попытки — ошибок 0
        // Если после всех попыток ошибка — считаем несовпадения
        total_info_bits += static_cast<std::size_t>(k);
        if (!success) {
            for (int i = 0; i < k; ++i) {
                if (final_decision[i] != info_word[i]) {
                    ++total_bit_errors;
                }
            }
        }
    }

    // Возвращаем BER
    if (total_info_bits == 0) return 1.0;
    return static_cast<double>(total_bit_errors) / static_cast<double>(total_info_bits);
}

int main() {
    std::cout << std::fixed << std::setprecision(6);
    
    // Заголовок CSV
    std::cout << "snr_db,"
              << "chase1_comb_ber,chase2_comb_ber,chase3_comb_ber,"
              << "chase1_nocomb_ber,chase2_nocomb_ber,chase3_nocomb_ber"
              << std::endl;

    for (double snr_db : snr_values) {
        // Симуляция для каждой конфигурации
        double c1  = simulate_ber(snr_db, harq::ProbeAlgorithm::First,  true);
        double c2  = simulate_ber(snr_db, harq::ProbeAlgorithm::Second, true);
        double c3  = simulate_ber(snr_db, harq::ProbeAlgorithm::Third,  true);
        double nc1 = simulate_ber(snr_db, harq::ProbeAlgorithm::First,  false);
        double nc2 = simulate_ber(snr_db, harq::ProbeAlgorithm::Second, false);
        double nc3 = simulate_ber(snr_db, harq::ProbeAlgorithm::Third,  false);

        std::cout << snr_db << ","
                  << c1  << "," << c2  << "," << c3  << ","
                  << nc1 << "," << nc2 << "," << nc3
                  << std::endl;
    }

    return 0;
}