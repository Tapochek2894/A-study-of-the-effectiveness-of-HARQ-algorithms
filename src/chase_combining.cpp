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
    const harq::HammingDecoder& decoder, std::vector<std::vector<double>> soft_bits) {

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

std::vector<uint8_t> ChaseCombiningConvNoCRC(harq::ProbeAlgorithm ProbeAlgorithm,
    std::unique_ptr<fec::IFecCodec>& codec, std::vector<std::vector<double>> soft_bits, int d) {

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

    return DecodeConvCodesWithChase(combined, ProbeAlgorithm, codec, d);
}

} // namespace harq
