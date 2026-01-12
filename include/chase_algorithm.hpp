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

std::vector<uint8_t> MakeDecision(std::vector<std::vector<uint8_t>> candidates,
                                  std::vector<double> SoftDecisions);

std::pair<double, std::vector<uint8_t>>
CalculateDistance(std::vector<uint8_t> candidate,
                  std::vector<double> SoftDecisions);

std::vector<std::vector<uint8_t>>
DecodeCandidatesByHamming(const std::vector<std::vector<uint8_t>> &candidates,
                          const HammingDecoder &decoder);

template <typename T>
std::vector<T> RemoveCheckBits(std::vector<T> codeword) {
  std::vector<T> NewVector;
  for (std::size_t i = 1; i < codeword.size() + 1; ++i) {
    if (!(i > 0 && (i & (i - 1)) == 0)) {
      NewVector.push_back(codeword[i - 1]);
    }
  }
  return NewVector;
}

std::vector<uint8_t> DecodingByChase();

} // namespace harq
