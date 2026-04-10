#pragma once

#include <vector>
#include <cstdint>
#include "hamming_decoder.hpp"
#include "chase_algorithm.hpp"

namespace harq {

std::vector<uint8_t> ChaseCombiningHammingNoCRC(
    ProbeAlgorithm probe_algorithm,
    const HammingDecoder& decoder,
    std::vector<std::vector<double>> soft_bits);

std::vector<uint8_t> ChaseCombiningConvNoCRC(harq::ProbeAlgorithm ProbeAlgorithm,
    std::unique_ptr<fec::IFecCodec>& codec, std::vector<std::vector<double>> soft_bits, int d);

} // namespace harq

