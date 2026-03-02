#include "awgn_channel.hpp"
#include "bpsk.hpp"
#include "chase_combining.hpp"
#include "hamming_decoder.hpp"
#include "hamming_encoder.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>

const int N = 5000;
const int r = 3;
const int MaximumAttempts = 10;
const uint32_t seed = 5489u;
const std::vector<double> snr_values = {
    -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
};

// Возвращает среднее число повторных передач.
// probe == nullopt → без комбинирования (каждая попытка независима)
double simulate(double snr_db, harq::ProbeAlgorithm algo, bool combining) {
    harq::HammingEncoder encoder(r);
    harq::HammingDecoder decoder(r);

    int k = (1 << r) - 1 - r;
    std::vector<uint8_t> info_word(k);
    for (int i = 0; i < k; ++i) info_word[i] = static_cast<uint8_t>(i % 2);
    auto codeword = encoder.Encode(info_word);
    auto modulated = harq::BpskModulate(codeword);

    std::size_t total_retransmits = 0;

    for (int i = 0; i < N; ++i) {
        std::vector<std::vector<double>> soft_history;
        int attempts = 0;

        for (int j = 0; j < MaximumAttempts; ++j) {
            harq::AwgnChannel channel(snr_db,
                static_cast<uint32_t>(seed + i * MaximumAttempts + j * j * j));
            auto soft_bits = channel.AddNoise(modulated);

            std::vector<uint8_t> decision;
            if (combining) {
                soft_history.push_back(soft_bits);
                decision = harq::ChaseCombiningHammingNoCRC(algo, decoder, soft_history);
            } else {
                decision = harq::DecodeHammingCodesWithChase(soft_bits, algo, decoder);
            }

            attempts = j + 1;
            bool error = false;
            for (std::size_t idx = 0; idx < info_word.size(); ++idx) {
                if (decision[idx] != info_word[idx]) { error = true; break; }
            }
            if (!error) break;
        }

        total_retransmits += static_cast<std::size_t>(attempts - 1);
    }

    return static_cast<double>(total_retransmits) / static_cast<double>(N);
}

int main() {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "snr_db,"
              << "chase1_combining,chase2_combining,chase3_combining,"
              << "chase1_no_comb,chase2_no_comb,chase3_no_comb"
              << std::endl;

    for (auto snr_db : snr_values) {
        double c1  = simulate(snr_db, harq::ProbeAlgorithm::First,  true);
        double c2  = simulate(snr_db, harq::ProbeAlgorithm::Second, true);
        double c3  = simulate(snr_db, harq::ProbeAlgorithm::Third,  true);
        double nc1 = simulate(snr_db, harq::ProbeAlgorithm::First,  false);
        double nc2 = simulate(snr_db, harq::ProbeAlgorithm::Second, false);
        double nc3 = simulate(snr_db, harq::ProbeAlgorithm::Third,  false);

        std::cout << snr_db << ","
                  << c1  << "," << c2  << "," << c3  << ","
                  << nc1 << "," << nc2 << "," << nc3
                  << std::endl;
    }

    return 0;
}
