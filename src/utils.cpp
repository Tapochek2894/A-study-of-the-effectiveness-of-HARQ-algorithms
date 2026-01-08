#include <algorithm>
#include <stdexcept>
#include "utils.hpp"

namespace harq {
std::vector<size_t> get_n_smallest_indices(const std::vector<double> &values,
                                           int n) {
  std::vector<size_t> result;

  if (n <= 0 || values.empty() || n > static_cast<int>(values.size())) {
    throw std::invalid_argument("Reliability values are incorrect.");
  }

  std::vector<std::pair<double, size_t>> indexed;
  indexed.reserve(values.size());

  for (size_t i = 0; i < values.size(); i++) {
    indexed.emplace_back(values[i], i);
  }

  std::partial_sort(
      indexed.begin(), indexed.begin() + n, indexed.end(),
      [](const auto &a, const auto &b) { return a.first < b.first; });

  result.reserve(n);
  for (int i = 0; i < n; i++) {
    result.push_back(indexed[i].second);
  }

  return result;
}

} // namespace harq
