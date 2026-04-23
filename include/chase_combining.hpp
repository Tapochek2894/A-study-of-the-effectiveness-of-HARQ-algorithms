#pragma once

#include <vector>
#include <cstdint>
#include "hamming_decoder.hpp"
#include "chase_algorithm.hpp"

namespace harq {

std::vector<uint8_t> ChaseCombiningHammingNoCRC(
    ProbeAlgorithm probe_algorithm,
    const HammingDecoder& decoder,
    const std::vector<std::vector<double>>& soft_bits);

std::vector<uint8_t> ChaseCombiningConvNoCRC(
    const std::unique_ptr<harq::fec::IFecCodec>& codec,
    const std::vector<std::vector<double>>& soft_bits);

} // namespace harq

