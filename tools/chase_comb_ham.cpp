#include "awgn_channel.hpp"
#include "bpsk.hpp"
#include "bpsk_passband.hpp"
#include "chase_combining.hpp"
#include "hamming_decoder.hpp"
#include "hamming_encoder.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>

const int N = 1000;
const int r = 3;                        
const int MaximumAttempts = 10;
const uint32_t seed = 5489u;
const std::vector<double> snr_values = {
    -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10,
    -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5
};

enum class Mode {
    ChaseCombining,
    NoCombining
};

double simulate_mode(double snr_db, Mode mode) {
    harq::BpskCarrierConfig config;
    config.carrier_hz = 2.0;
    config.sample_rate_hz = 32.0;
    config.samples_per_symbol = 4;
    config.amplitude = 1.0;
    config.phase = 0.0;

    harq::HammingEncoder encoder(r);
    harq::HammingDecoder decoder(r);

    std::vector<uint8_t> info_word = {1, 0, 1, 0};
    auto codeword = encoder.Encode(info_word);

    std::size_t total_retransmits = 0;

    for (int i = 0; i < N; ++i) {
        auto modulated = harq::BpskModulate(codeword);
        std::vector<std::vector<double>> soft_history;

        int attempts = 0;

        for (int j = 0; j < MaximumAttempts; ++j) {
            harq::AwgnChannel channel(snr_db, static_cast<uint32_t>(seed + i * MaximumAttempts + j * j * j));
            auto noisy = channel.AddNoise(modulated);
            auto soft_bits = noisy;

            std::vector<uint8_t> decision;

            if (mode == Mode::ChaseCombining) {
                soft_history.push_back(soft_bits);
                decision = harq::ChaseCombiningHammingNoCRC(harq::ProbeAlgorithm::Full, decoder, soft_history);
            } else {
                decision = harq::DecodeHammingCodesWithChase(
                    soft_bits, harq::ProbeAlgorithm::Full, decoder
                );
            }
            bool error = false;
            for (std::size_t idx = 0; idx < info_word.size(); ++idx) {
                if (decision[idx] != info_word[idx]) {
                    error = true;
                }
            }

            attempts = j + 1;

            if (!error) {
                break;
            }
        }

        total_retransmits += (attempts - 1);
    }

    return static_cast<double>(total_retransmits) / static_cast<double>(N);
}

int main() {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "snr_db,chase_combining,no_combining" << std::endl;

    for (auto snr_db : snr_values) {
        double avg_chase = simulate_mode(snr_db, Mode::ChaseCombining);
        double avg_no_comb = simulate_mode(snr_db, Mode::NoCombining);
        std::cout << snr_db << "," << avg_chase << "," << avg_no_comb << std::endl;
    }

    return 0;
}