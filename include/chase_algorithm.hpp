#pragma once

#include <vector>

namespace harq {
std::vector<std::vector<char>> generate_probe_sequences_1(int n, int d);

std::vector<std::vector<char>>
generate_probe_sequences_2(int n, int d,
                           const std::vector<double> &reliability);

std::vector<std::vector<char>>
generate_probe_sequences_3(int n, int d,
                           const std::vector<double> &reliability);
} // namespace harq