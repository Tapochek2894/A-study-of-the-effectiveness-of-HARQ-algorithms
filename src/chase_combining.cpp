#include "chase_combining.hpp"
#include <cassert>
#include <iostream>
#include <map>
#include <bpsk.hpp>
#include <limits>

namespace harq {

struct candidate {
  std::size_t index;
  double weight;
};


std::vector<uint8_t> ChaseCombiningHammingNoCRC(harq::ProbeAlgorithm ProbeAlgorithm,
    const harq::HammingDecoder& decoder, const std::vector<std::vector<double>>& soft_bits) {

    std::size_t n = soft_bits[0].size();
    std::vector<double> combined(n, 0.0);
    for (const auto& transmission : soft_bits) {
        for (std::size_t i = 0; i < n; ++i) {
            combined[i] += transmission[i];
        }
    }

    for (auto& i : combined) {
        i /= soft_bits.size();
    }

    return DecodeHammingCodesWithChase(combined, ProbeAlgorithm, decoder);
}

std::vector<uint8_t> ChaseCombiningConvNoCRC(
    const std::unique_ptr<harq::fec::IFecCodec>& codec,
    const std::vector<std::vector<double>>& soft_bits)
{
    std::size_t n = soft_bits[0].size();
    std::vector<double> combined(n, 0.0);
    for (const auto& transmission : soft_bits) {
        for (std::size_t i = 0; i < n; ++i) {
            combined[i] += transmission[i];
        }
    }

    for (auto& i : combined) {
        i /= soft_bits.size();
    }

    return codec->DecodeSoft(combined);
}

} // namespace harq
