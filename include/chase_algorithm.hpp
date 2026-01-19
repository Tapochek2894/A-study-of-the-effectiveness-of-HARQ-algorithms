#pragma once

#include "hamming_decoder.hpp"
#include <cstdint>
#include <vector>

namespace harq {

const int HAMMING_CODE_DISTANCE = 3;

enum class ProbeAlgorithm { First, Second, Third };

std::vector<std::vector<uint8_t>> generate_probe_sequences_1(int n, int d);

std::vector<std::vector<uint8_t>>
generate_probe_sequences_2(int n, int d,
                           const std::vector<double> &reliability);

std::vector<std::vector<uint8_t>>
generate_probe_sequences_3(int n, int d,
                           const std::vector<double> &reliability);

std::vector<uint8_t> AddErrorVector(const std::vector<uint8_t> &DataVector,
                                    const std::vector<uint8_t> &ErrorVector);

std::vector<std::vector<uint8_t>>
CalculateCandidates(const std::vector<uint8_t> &message, int r, int d,
                    const std::vector<double> &reliability,
                    ProbeAlgorithm algorithm);

std::pair<double, std::vector<uint8_t>>
CalculateDistance(std::vector<uint8_t> candidate,
                  std::vector<double> SoftDecisions);

std::vector<std::size_t> get_n_smallest_indices(const std::vector<double> &values,
                                           int n);

std::vector<uint8_t> DecodeWithChase(std::vector<double> ReceivedSoftBits,
                                     harq::ProbeAlgorithm ProbeAlgorithm,
                                     harq::HammingDecoder decoder);

} // namespace harq
