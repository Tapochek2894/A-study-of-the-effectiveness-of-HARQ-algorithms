#include "awgn_channel.hpp"
#include "chase_combining.hpp"
#include "fec/fec_factory.hpp"
#include "crc.hpp"
#include "qpsk.hpp"

#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>

const int N = 10000;
const int MaximumAttempts = 10;
const uint32_t seed = 53u;

const int CONV_K = 256;
const int CONV_RATE_NUM = 1;
const int CONV_RATE_DEN = 2;

const std::vector<double> snr_values = {
    -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2
};

enum class CrcType { k24 };

harq::Crc CreateCrcByType(CrcType type)
{
    switch (type) {
        case CrcType::k24:
            return harq::Crc({
                1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                1,1,0,0,1,1,0,1,1,1
            });
    }
    return harq::Crc({1});
}

bool IsCrcOk(const harq::Crc& crc,
             const std::vector<uint8_t>& decoded,
             std::size_t expected_size)
{
    if (decoded.empty()) return false;
    if (crc.HasError(decoded)) return false;

    auto extracted = crc.Decode(decoded);
    return extracted.size() == expected_size;
}

bool Compare(const std::vector<uint8_t>& a,
             const std::vector<uint8_t>& b)
{
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin());
}

struct Metrics {
    double bler;
    double undetected;
    double goodput;
};

Metrics simulate(double snr_db, bool combining)
{
    harq::Crc crc = CreateCrcByType(CrcType::k24);

    harq::fec::FecConfig cfg{};
    cfg.codec_type = harq::fec::CodecType::kConvolutionalAff3ct;
    cfg.conv_input_bits_per_frame = CONV_K;
    cfg.conv_rate_num = CONV_RATE_NUM;
    cfg.conv_rate_den = CONV_RATE_DEN;
    cfg.conv_decoder = harq::fec::ConvDecoderType::kViterbi;

    auto codec = harq::fec::CreateCodec(cfg);

    const size_t total_size = codec->input_bits_per_frame();
    const size_t crc_bits = crc.r();
    const size_t info_size = total_size - crc_bits;

    std::mt19937 rng(seed);

    size_t crc_fail = 0;
    size_t undetected = 0;
    size_t success_blocks = 0;
    size_t total_channel_bits = 0;

    for (int i = 0; i < N; ++i) {

        std::vector<uint8_t> info(info_size);
        for (auto& b : info) b = rng() & 1;

        auto crc_encoded = crc.Encode(info);
        auto coded = codec->Encode(crc_encoded);
        auto modulated = harq::QpskModulate(coded);

        std::vector<std::vector<double>> history;

        bool crc_passed = false;
        bool correct = false;

        for (int attempt = 0; attempt < MaximumAttempts; ++attempt) {

            total_channel_bits += coded.size();

            harq::AwgnChannel channel(
                snr_db,
                seed + i * 100 + attempt
            );

            auto soft = harq::QpskDemodulate(
                channel.TransmitComplex(modulated), snr_db);

            std::vector<uint8_t> decoded;

            if (combining) {
                history.push_back(soft);
                decoded = harq::ChaseCombiningConvNoCRC(codec, history);
            } else {
                decoded = codec->DecodeSoft(soft);
            }

            if (IsCrcOk(crc, decoded, info_size)) {
                crc_passed = true;

                auto extracted = crc.Decode(decoded);
                correct = Compare(extracted, info);

                break;
            }
        }

        if (crc_passed) success_blocks++;
        else crc_fail++;

        if (crc_passed && !correct) undetected++;
    }

    double goodput =
        static_cast<double>(success_blocks * info_size) /
        static_cast<double>(total_channel_bits);

    return {
        (double)crc_fail / N,
        (double)undetected / N,
        goodput
    };
}

int main()
{
    std::cout << std::fixed << std::setprecision(6);

    std::cout << "snr,"
              << "bler_comb,und_comb,gput_comb,"
              << "bler_no,und_no,gput_no\n";

    for (auto snr : snr_values) {

        auto comb = simulate(snr, true);
        auto no   = simulate(snr, false);

        std::cout << snr << ","
                  << comb.bler << "," << comb.undetected << "," << comb.goodput << ","
                  << no.bler   << "," << no.undetected   << "," << no.goodput
                  << "\n";

        std::cerr << "SNR " << snr << " done\n";
    }

    return 0;
}