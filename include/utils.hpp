#pragma once

#include <vector>

namespace harq {
std::vector<size_t> get_n_smallest_indices(const std::vector<double> &values,
                                           int n);
} // namespace harq