#pragma once

#include <cstdint>
#include <vector>
#include "hamming_decoder.hpp"

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

// Вычисляет мягкое евклидово расстояние между кодовым словом и принятыми символами.
// codeword — бинарное кодовое слово, soft_bits — принятые BPSK-символы (0→−1, 1→+1).
double ComputeSoftDistance(const std::vector<uint8_t>& codeword,
                           const std::vector<double>& soft_bits);

// Декодирует кодовое слово Хэмминга алгоритмом Чейза.
// received_soft_bits — мягкие решения (принятые BPSK-символы) длины n.
// probe_algorithm    — стратегия генерации пробных последовательностей.
// decoder            — декодер Хэмминга с заданным r.
// Возвращает k информационных бит наилучшего кандидата.
std::vector<uint8_t> DecodeHammingCodesWithChase(
    const std::vector<double>& received_soft_bits,
    ProbeAlgorithm probe_algorithm,
    HammingDecoder decoder);

} // namespace harq