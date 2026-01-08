#include "chase_algorithm.hpp"
#include "utils.hpp"
#include <algorithm>
#include <stdexcept>

namespace harq {
std::vector<std::vector<char>> generate_probe_sequences_1(int n, int d) {
  std::vector<std::vector<char>> result;
  int ones_count = d / 2;

  if (ones_count == 0) {
    throw std::invalid_argument("Wrong input data: d should be >= 0");
  }

  if (ones_count > n) {
    throw std::invalid_argument("Wrong input data: d/2 > n");
  }

  result.push_back(std::vector<char>(n, 0));
  std::vector<char> mask(n, 0);
  std::fill(mask.end() - ones_count, mask.end(), 1);

  do {
    result.push_back(mask);
  } while (std::next_permutation(mask.begin(), mask.end()));

  return result;
}

std::vector<std::vector<char>>
generate_probe_sequences_2(int n, int d,
                           const std::vector<double> &reliability) {

  std::vector<std::vector<char>> result;

  if (n <= 0 || d <= 0 || reliability.size() != static_cast<size_t>(n)) {
    throw std::invalid_argument("Reliability values are incorrect.");
  }

  int selection_positions = d / 2;
  if (selection_positions > n) {
    throw std::invalid_argument("Wrong input data: d/2 > n");
  }
  if (selection_positions == 0) {
    result.push_back(std::vector<char>(n, 0));
    return result;
  }

  auto min_indices = get_n_smallest_indices(reliability, selection_positions);

  int total_combinations = 1 << selection_positions;

  std::vector<char> base_sequence(n, 0);

  for (int mask = 0; mask < total_combinations; mask++) {
    std::vector<char> sequence = base_sequence;
    for (int i = 0; i < selection_positions; i++) {
      char value = (mask >> i) & 1;
      sequence[min_indices[i]] = value;
    }

    result.push_back(sequence);
  }

  return result;
}

std::vector<std::vector<char>>
generate_probe_sequences_3(int n, int d,
                           const std::vector<double> &reliability) {

  std::vector<std::vector<char>> result;

  if (n <= 0 || d <= 0 || reliability.size() != static_cast<size_t>(n)) {
    throw std::invalid_argument("Reliability values are incorrect.");
    return result;
  }

  int precarious_positions = d - 1;
  if (precarious_positions > n) {
    throw std::invalid_argument("Wrong input data: d-1 > n");
  }

  auto least_reliable_indices =
      get_n_smallest_indices(reliability, precarious_positions);

  std::vector<size_t> ones_positions;

  if (d % 2 == 1) {
    for (size_t i = 0; i < least_reliable_indices.size(); i += 2) {
      ones_positions.push_back(least_reliable_indices[i]);
    }
  } else {
    if (least_reliable_indices.size() >= 1) {
      ones_positions.push_back(least_reliable_indices[0]);
    }
    if (least_reliable_indices.size() >= 2) {
      ones_positions.push_back(least_reliable_indices[1]);
    }
    for (size_t i = 3; i < least_reliable_indices.size(); i += 2) {
      ones_positions.push_back(least_reliable_indices[i]);
    }
  }

  std::vector<char> sequence(n, 0);
  for (size_t pos : ones_positions) {
    if (pos < static_cast<size_t>(n)) {
      sequence[pos] = 1;
    }
  }

  result.push_back(sequence);
  return result;
}

} // namespace harq
