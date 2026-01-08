#include <algorithm>
#include <stdexcept>
#include "utils.hpp"

namespace harq {
std::vector<size_t> get_n_smallest_indices(const std::vector<double> &soft_desicions,
                                           int n) {
  std::vector<size_t> result;

  if (n <= 0 || soft_desicions.empty() || n > static_cast<int>(soft_desicions.size())) {
    throw std::invalid_argument("Reliability values are incorrect.");
  }

  std::vector<std::pair<double, size_t>> indexed;
  indexed.reserve(soft_desicions.size());

  for (size_t i = 0; i < soft_desicions.size(); i++) {
    indexed.emplace_back(soft_desicions[i], i);
  }

  std::partial_sort(
      indexed.begin(), indexed.begin() + n, indexed.end(),
      [](const auto &a, const auto &b) { return std::abs(a.first) < std::abs(b.first); });

  result.reserve(n);
  for (int i = 0; i < n; i++) {
    result.push_back(indexed[i].second);
  }

  return result;
}

} // namespace harq
