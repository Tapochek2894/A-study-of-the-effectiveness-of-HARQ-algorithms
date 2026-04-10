#include "awgn_channel.hpp"
#include "qpsk.hpp"  // или bpsk.hpp, если нужна BPSK
#include "chase_combining.hpp"
#include "bpsk.hpp"
#include "fec/fec_factory.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#include <memory>

const int N = 2000;           // число независимых испытаний
const int MaximumAttempts = 10;
const uint32_t seed = 53u;

// Параметры свёрточного кода
const int CONV_K = 64;                    // информационных бит на фрейм
const int CONV_RATE_NUM = 1;               // скорость кода: num/den
const int CONV_RATE_DEN = 2;
const int CONV_DFREE = 10;                 // свободное расстояние (для Chase)

const std::vector<double> snr_values = {
    -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5
};

double simulate(double snr_db, harq::ProbeAlgorithm algo, bool combining) {
    harq::fec::FecConfig cfg{};
    cfg.codec_type = harq::fec::CodecType::kConvolutionalAff3ct;
    cfg.conv_input_bits_per_frame = static_cast<std::size_t>(CONV_K);
    cfg.conv_rate_num = CONV_RATE_NUM;
    cfg.conv_rate_den = CONV_RATE_DEN;
    cfg.conv_decoder = harq::fec::ConvDecoderType::kViterbi;

    auto codec = harq::fec::CreateCodec(cfg);
    if (!codec) {
        std::cerr << "Error: Failed to create convolutional codec at SNR " << snr_db << "\n";
        return static_cast<double>(MaximumAttempts - 1);
    }

    const std::size_t input_size = codec->input_bits_per_frame();

    std::vector<uint8_t> info_word(input_size);
    for (std::size_t i = 0; i < input_size; ++i)
        info_word[i] = static_cast<uint8_t>(i % 2);

    // Кодируем один раз
    auto coded_bits = codec->Encode(info_word);
    auto modulated = harq::BpskModulate(coded_bits); 

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
                decision = harq::ChaseCombiningConvNoCRC(algo, codec, soft_history, CONV_DFREE);
            } else {
                decision = harq::DecodeConvCodesWithChase(soft_bits, algo, codec, CONV_DFREE);
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
              << "chase2_combining,chase3_combining,"
              << "chase2_no_comb,chase3_no_comb"
              << std::endl;

    for (auto snr_db : snr_values) {
        double c2  = simulate(snr_db, harq::ProbeAlgorithm::Second, true);
        double c3  = simulate(snr_db, harq::ProbeAlgorithm::Third,  true);
        double nc2 = simulate(snr_db, harq::ProbeAlgorithm::Second, false);
        double nc3 = simulate(snr_db, harq::ProbeAlgorithm::Third,  false);

        std::cout << snr_db << "," << c2  << "," << c3  << "," << nc2 << "," << nc3 << std::endl;
        
        std::cerr << "SNR " << snr_db << " dB done.\n";
    }

    return 0;
}