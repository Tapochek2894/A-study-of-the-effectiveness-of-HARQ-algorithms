#pragma once

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

std::vector<uint8_t> MakeDecision(std::vector<std::vector<uint8_t>> candidates,
                                  std::vector<double> SoftDecisions);

std::pair<double, std::vector<uint8_t>>
CalculateDistance(std::vector<uint8_t> candidate,
                  std::vector<double> SoftDecisions);
} // namespace harq
