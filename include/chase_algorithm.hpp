#pragma once

#include "hamming_decoder.hpp"
#include <cstdint>
#include <vector>

namespace harq {

const int HAMMING_CODE_DISTANCE = 3;
const int EXTENDED_HAMMING_CODE_DISTANCE = 4;

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
CalculateCandidates(const std::vector<uint8_t> &message, int d,
                    const std::vector<double> &reliability,
                    ProbeAlgorithm algorithm);

std::pair<double, std::vector<uint8_t>>
CalculateDistance(std::vector<uint8_t> candidate,
                  std::vector<double> SoftDecisions);

std::vector<std::size_t> get_n_smallest_indices(const std::vector<double> &values,
                                           int n);

std::vector<uint8_t> DecodeHammingCodesWithChase(
    const std::vector<double>& ReceivedSoftBits,
    harq::ProbeAlgorithm probe_algorithm,
    harq::HammingDecoder decoder);

inline bool IsPowerOfTwo(std::size_t n);

double ComputeSoftDistance(const std::vector<uint8_t>& codeword,
                           const std::vector<double>& soft_bits);

int GetCodeDistance(std::size_t received_size);

std::vector<uint8_t> DecodeHammingML(
    const std::vector<double>& soft_bits,
    const HammingDecoder& decoder);

} // namespace harq
