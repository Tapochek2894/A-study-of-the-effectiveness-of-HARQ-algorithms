#pragma once

#include <vector>
#include <cstdint>
#include "hamming_decoder.hpp"

namespace harq {

enum class ProbeAlgorithm {
    First,
    Second,
    Third,
    Full
};

const int EXTENDED_HAMMING_CODE_DISTANCE = 4;
const int HAMMING_CODE_DISTANCE = 3;

std::vector<std::vector<uint8_t>> generate_probe_sequences_1(int n, int d);
std::vector<std::vector<uint8_t>> generate_probe_sequences_2(int n, int d, const std::vector<double>& reliability);
std::vector<std::vector<uint8_t>> generate_probe_sequences_3(int n, int d, const std::vector<double>& reliability);
std::vector<std::vector<uint8_t>> generate_probe_sequences_ml(int n, int d, int r);

std::vector<uint8_t> AddErrorVector(const std::vector<uint8_t>& data, const std::vector<uint8_t>& error);
std::vector<std::size_t> get_n_smallest_indices(const std::vector<double>& soft_decisions, int n);
std::vector<std::vector<uint8_t>> CalculateCandidates(
    const std::vector<uint8_t>& message, int r, int d,
    const std::vector<double>& reliability, ProbeAlgorithm algorithm);

double ComputeSoftDistance(const std::vector<uint8_t>& codeword, const std::vector<double>& soft_bits);

std::vector<uint8_t> DecodeHammingCodesWithChase(
    const std::vector<double>& received_soft_bits,
    ProbeAlgorithm probe_algorithm,
    HammingDecoder decoder);

std::vector<uint8_t> DecodeHammingML(
    const std::vector<double>& soft_bits,
    const HammingDecoder& decoder);

} // namespace harq
