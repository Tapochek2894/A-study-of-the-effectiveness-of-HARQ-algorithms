#include "chase_algorithm.hpp"
#include "utils.hpp"
#include <algorithm>
#include <stdexcept>

namespace harq {
std::vector<std::vector<uint8_t>> generate_probe_sequences_1(int n, int d) {
  std::vector<std::vector<uint8_t>> result;
  int ones_count = d / 2;

  if (ones_count == 0) {
    throw std::invalid_argument("Wrong input data: d should be >= 0");
  }

  if (ones_count > n) {
    throw std::invalid_argument("Wrong input data: d/2 > n");
  }

  result.push_back(std::vector<uint8_t>(n, 0));
  std::vector<uint8_t> mask(n, 0);
  std::fill(mask.end() - ones_count, mask.end(), 1);

  do {
    result.push_back(mask);
  } while (std::next_permutation(mask.begin(), mask.end()));

  return result;
}

std::vector<std::vector<uint8_t>>
generate_probe_sequences_2(int n, int d,
                           const std::vector<double> &reliability) {

  std::vector<std::vector<uint8_t>> result;

  if (n <= 0 || d <= 0 || reliability.size() != static_cast<size_t>(n)) {
    throw std::invalid_argument("Reliability values are incorrect.");
  }

  int selection_positions = d / 2;
  if (selection_positions > n) {
    throw std::invalid_argument("Wrong input data: d/2 > n");
  }
  if (selection_positions == 0) {
    result.push_back(std::vector<uint8_t>(n, 0));
    return result;
  }

  auto min_indices = get_n_smallest_indices(reliability, selection_positions);

  int total_combinations = 1 << selection_positions;

  std::vector<uint8_t> base_sequence(n, 0);

  for (int mask = 0; mask < total_combinations; mask++) {
    std::vector<uint8_t> sequence = base_sequence;
    for (int i = 0; i < selection_positions; i++) {
      char value = (mask >> i) & 1;
      sequence[min_indices[i]] = value;
    }

    result.push_back(sequence);
  }

  return result;
}

std::vector<std::vector<uint8_t>>
generate_probe_sequences_3(int n, int d,
                           const std::vector<double> &reliability) {
  std::vector<std::vector<uint8_t>> result;

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

  std::vector<uint8_t> sequence(n, 0);
  for (size_t pos : ones_positions) {
    if (pos < static_cast<size_t>(n)) {
      sequence[pos] = 1;
    }
  }

  result.push_back(sequence);
  return result;
}

std::vector<uint8_t> AddErrorVector(const std::vector<uint8_t> &DataVector,
                                    const std::vector<uint8_t> &ErrorVector) {
  std::vector<uint8_t> NewVector(DataVector.size(), 0);
  for (auto i = 0; i < NewVector.size(); ++i) {
    NewVector[i] = DataVector[i] ^ ErrorVector[i];
  }
  return NewVector;
}

std::vector<std::vector<uint8_t>>
CalculateCandidates(const std::vector<uint8_t> &message, int r, int d,
                    const std::vector<double> &reliability,
                    ProbeAlgorithm algorithm) {
  (void)r;

  if (message.empty()) {
    throw std::invalid_argument("Message must be non-empty.");
  }

  std::vector<std::vector<uint8_t>> ProbeSeqs;
  switch (algorithm) {
  case ProbeAlgorithm::First:
    ProbeSeqs = generate_probe_sequences_1(message.size(), d);
    break;
  case ProbeAlgorithm::Second:
    ProbeSeqs = generate_probe_sequences_2(message.size(), d, reliability);
    break;
  case ProbeAlgorithm::Third:
    ProbeSeqs = generate_probe_sequences_3(message.size(), d, reliability);
    break;
  default:
    throw std::invalid_argument("Wrong probe algorithm chosen");
  }

  std::vector<std::vector<uint8_t>> CandidatesVector;
  CandidatesVector.reserve(ProbeSeqs.size());
  for (const auto &ErrorVector : ProbeSeqs) {
    CandidatesVector.push_back(AddErrorVector(message, ErrorVector));
  }
  return CandidatesVector;
}

std::pair<double, std::vector<uint8_t>>
CalculateDistance(std::vector<uint8_t> candidate,
                  std::vector<double> SoftDecisions) {
  double result = 0;
  for (auto i = 0; i < candidate.size(); ++i) {
    result += std::abs(static_cast<double>(candidate[i]) - SoftDecisions[i]);
  }
  return {result, candidate};
}

std::vector<uint8_t> MakeDecision(std::vector<std::vector<uint8_t>> candidates,
                                  std::vector<double> SoftDecisions) {
  std::vector<std::pair<double, std::vector<uint8_t>>> distances;
  for (const auto &candidate : candidates) {
    auto CandidateWithDistance = CalculateDistance(candidate, SoftDecisions);
    distances.push_back(CandidateWithDistance);
  }
  std::sort(distances.begin(), distances.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  return distances[0].second;
}
} // namespace harq
