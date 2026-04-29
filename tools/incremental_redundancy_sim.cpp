#include "awgn_channel.hpp"
#include "qpsk.hpp"
#include "fec/fec_factory.hpp"
#include "crc.hpp"
#include "incremental_redundancy.hpp"

#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>
#include <memory>
#include <stdexcept>

const int N = 20000;
const uint32_t seed = 53u;

const int CONV_K = 256;
const int CONV_RATE_NUM = 1;
const int CONV_RATE_DEN = 3;
const int INITIAL_TX_RATE_NUM = 1;
const int INITIAL_TX_RATE_DEN = 2;
const std::size_t IR_REDUNDANCY_VERSIONS = 4;
const int MAX_TRANSMISSIONS = 4;

const std::vector<double> snr_values = {
    -10, -9.5, -9, -8.5, -8, -7.5, -7, -6.5, -6, 
    -5.5, -5, -4.5, -4, -3.5, -3, -2.5, -2, -1.5, 
    -1, -0.5, 0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4
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

enum class Scheme {
    kNoHarq,
    kChaseFull,
    kChasePunctured,
    kIncrementalRedundancy
};

std::vector<std::size_t> BuildRateHalfPuncturingOrderForRateThird(
    std::size_t codeword_size
) {
    if (codeword_size % 3 != 0) {
        throw std::runtime_error("Rate 1/3 convolutional codeword size must be divisible by 3.");
    }

    const std::size_t trellis_steps = codeword_size / 3;
    std::vector<std::size_t> order;
    order.reserve(codeword_size);

    for (std::size_t step = 0; step < trellis_steps; ++step) {
        order.push_back(3 * step);
        order.push_back(3 * step + 1);
    }
    for (std::size_t step = 0; step < trellis_steps; ++step) {
        order.push_back(3 * step + 2);
    }

    return order;
}

std::vector<uint8_t> DecodeFullChase(
    const harq::fec::IFecCodec& codec,
    const std::vector<std::vector<double>>& history
) {
    std::vector<double> combined(history.front().size(), 0.0);

    for (const auto& soft : history) {
        for (std::size_t i = 0; i < soft.size(); ++i) {
            combined[i] += soft[i];
        }
    }

    return codec.DecodeSoft(combined);
}

std::vector<uint8_t> DecodePuncturedChase(
    const harq::fec::IFecCodec& codec,
    const std::vector<std::vector<double>>& history,
    std::size_t codeword_size,
    const std::vector<std::size_t>& perm
) {
    std::vector<double> combined(codeword_size, 0.0);

    for (const auto& soft : history) {
        auto dematched = harq::RateDematch(
            soft, 0, codeword_size, perm, IR_REDUNDANCY_VERSIONS);
        for (std::size_t i = 0; i < codeword_size; ++i) {
            combined[i] += dematched[i];
        }
    }

    return codec.DecodeSoft(combined);
}

std::vector<uint8_t> DecodeSinglePunctured(
    const harq::fec::IFecCodec& codec,
    const std::vector<double>& soft,
    std::size_t codeword_size,
    const std::vector<std::size_t>& perm
) {
    return codec.DecodeSoft(harq::RateDematch(
        soft, 0, codeword_size, perm, IR_REDUNDANCY_VERSIONS));
}

Metrics simulate(double snr_db, Scheme scheme)
{
    harq::Crc crc = CreateCrcByType(CrcType::k24);

    harq::fec::FecConfig cfg{};
    cfg.codec_type = harq::fec::CodecType::kConvolutional;
    cfg.conv_input_bits_per_frame = CONV_K;
    cfg.conv_rate_num = CONV_RATE_NUM;
    cfg.conv_rate_den = CONV_RATE_DEN;
    cfg.conv_generators = {0133, 0171, 0165};

    std::unique_ptr<harq::fec::IFecCodec> codec =
        harq::fec::CreateCodec(cfg);

    const size_t fec_input_size = codec->input_bits_per_frame();
    const size_t codeword_size = codec->output_bits_per_frame();
    const size_t crc_bits = crc.r();
    if (fec_input_size <= crc_bits) {
        throw std::runtime_error("FEC input frame is too small for the selected CRC.");
    }
    const size_t info_size = fec_input_size - crc_bits;

    const auto perm = BuildRateHalfPuncturingOrderForRateThird(codeword_size);
    const std::size_t bits_per_transmission =
        codeword_size * INITIAL_TX_RATE_DEN * CONV_RATE_NUM /
        (INITIAL_TX_RATE_NUM * CONV_RATE_DEN);

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

        std::vector<std::vector<double>> history;

        bool success = false;
        bool correct = false;

        for (int attempt = 0; attempt < MAX_TRANSMISSIONS; ++attempt) {

            harq::AwgnChannel channel(
                snr_db,
                seed + i * 100 + attempt
            );

            std::vector<double> soft;
            std::size_t transmitted_bits = 0;

            if (scheme == Scheme::kChaseFull) {
                auto mod = harq::QpskModulate(coded);
                soft = harq::QpskDemodulate(channel.TransmitComplex(mod), snr_db);
                transmitted_bits = coded.size();
            }
            else if (scheme == Scheme::kIncrementalRedundancy) {
                auto tx_bits = harq::RateMatch(
                    coded, attempt, bits_per_transmission, perm,
                    IR_REDUNDANCY_VERSIONS);
                auto mod = harq::QpskModulate(tx_bits);
                soft = harq::QpskDemodulate(channel.TransmitComplex(mod), snr_db);
                transmitted_bits = tx_bits.size();
            } else {
                auto tx_bits = harq::RateMatch(
                    coded, 0, bits_per_transmission, perm,
                    IR_REDUNDANCY_VERSIONS);
                auto mod = harq::QpskModulate(tx_bits);
                soft = harq::QpskDemodulate(channel.TransmitComplex(mod), snr_db);
                transmitted_bits = tx_bits.size();
            }

            total_channel_bits += transmitted_bits;

            std::vector<uint8_t> decoded;

            if (scheme == Scheme::kNoHarq) {
                decoded = DecodeSinglePunctured(*codec, soft, coded.size(), perm);
            }
            else if (scheme == Scheme::kChaseFull) {
                history.push_back(soft);
                decoded = DecodeFullChase(*codec, history);
            }
            else if (scheme == Scheme::kChasePunctured) {
                history.push_back(soft);
                decoded = DecodePuncturedChase(*codec, history, coded.size(), perm);
            }
            else if (scheme == Scheme::kIncrementalRedundancy) {
                history.push_back(soft);
                decoded = harq::DecodeIncrementalRedundancy(
                    *codec, history, coded.size(), perm, IR_REDUNDANCY_VERSIONS);
            }
            else {
                throw std::runtime_error("Unknown simulation scheme.");
            }

            if (IsCrcOk(crc, decoded, info_size)) {
                success = true;
                auto extracted = crc.Decode(decoded);
                correct = Compare(extracted, info);
                break;
            }
        }

        if (success) success_blocks++;
        else crc_fail++;

        if (success && !correct) undetected++;
    }

    const double goodput =
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
              << "bler_no,und_no,gput_no,"
              << "bler_chase_full,und_chase_full,gput_chase_full,"
              << "bler_chase_punctured,und_chase_punctured,gput_chase_punctured,"
              << "bler_ir,und_ir,gput_ir"
              << "\n";

    for (auto snr : snr_values) {

        auto no = simulate(snr, Scheme::kNoHarq);
        auto chase_full = simulate(snr, Scheme::kChaseFull);
        auto chase_punctured = simulate(snr, Scheme::kChasePunctured);
        auto ir = simulate(snr, Scheme::kIncrementalRedundancy);

        std::cout << snr << ","
                  << no.bler << "," << no.undetected << "," << no.goodput << ","
                  << chase_full.bler << "," << chase_full.undetected << "," << chase_full.goodput << ","
                  << chase_punctured.bler << "," << chase_punctured.undetected << "," << chase_punctured.goodput << ","
                  << ir.bler << "," << ir.undetected << "," << ir.goodput
                  << "\n";

        std::cerr << "SNR " << snr << " done\n";
    }

    return 0;
}