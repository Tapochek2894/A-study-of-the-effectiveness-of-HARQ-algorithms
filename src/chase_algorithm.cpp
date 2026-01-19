#include "chase_algorithm.hpp"
#include "hamming_decoder.hpp"
#include <algorithm>
#include <stdexcept>
#include <limits>
#include "hamming_encoder.hpp"

namespace harq {

std::vector<std::vector<uint8_t>> generate_probe_sequences_1(int n, int d) {
    std::vector<std::vector<uint8_t>> result;

    // Проверка входных данных
    if (n <= 0) {
        throw std::invalid_argument("n must be positive");
    }
    if (d < 0) {
        throw std::invalid_argument("d must be non-negative");
    }

    int ones_count = d / 2;  // floor(d/2)

    if (ones_count > n) {
        throw std::invalid_argument("floor(d/2) cannot exceed n");
    }

    // Шаг 1: добавляем нулевой вектор
    result.push_back(std::vector<uint8_t>(n, 0));

    // Если ones_count == 0, больше ничего не нужно (например, d=0 или d=1)
    if (ones_count == 0) {
        return result;
    }

    // Шаг 2: генерируем все векторы веса ones_count
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

  if (n <= 0 || d <= 0 || reliability.size() != static_cast<std::size_t>(n)) {
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

  auto min_indices = harq::get_n_smallest_indices(reliability, selection_positions);

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

  if (n <= 0 || d <= 0 || reliability.size() != static_cast<std::size_t>(n)) {
    throw std::invalid_argument("Reliability values are incorrect.");
  }
  int precarious_positions = d - 1;
  if (precarious_positions > n) {
    throw std::invalid_argument("Wrong input data: d-1 > n");
  }

  if (d % 2 == 1) {
    for (std::size_t i = 0; i <= precarious_positions; i += 2) {
      std::vector<uint8_t> sequence(n, 0);
      if (i != 0) {
        auto least_reliable_indices = harq::get_n_smallest_indices(reliability, i);
        for (std::size_t i = 0; i < least_reliable_indices.size(); ++i) {
          sequence[least_reliable_indices[i]] = 1;
        }
      }
      result.push_back(sequence);
    }
  } else {
    std::vector<uint8_t> sequence(n, 0);
    result.push_back(sequence);
    for (std::size_t i = 1; i <= precarious_positions; i += 2) {
      std::vector<uint8_t> sequence(n, 0);
      if (i != 0) {
        auto least_reliable_indices = harq::get_n_smallest_indices(reliability, i);
        for (std::size_t i = 0; i < least_reliable_indices.size(); ++i) {
          sequence[least_reliable_indices[i]] = 1;
        }
      }
      result.push_back(sequence);
    }
  }

  return result;
}

std::vector<uint8_t> AddErrorVector(const std::vector<uint8_t> &DataVector,
                                    const std::vector<uint8_t> &ErrorVector) {
  std::vector<uint8_t> NewVector(DataVector.size(), 0);
  for (std::size_t i = 0; i < NewVector.size(); ++i) {
    NewVector[i] = DataVector[i] ^ ErrorVector[i];
  }
  return NewVector;
}

std::vector<std::size_t> get_n_smallest_indices(const std::vector<double> &soft_desicions,
                                           int n) {
  std::vector<std::size_t> result;

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

std::vector<uint8_t> DecodeWithChase(std::vector<double> ReceivedSoftBits,
                                     harq::ProbeAlgorithm probe_algorithm,
                                     harq::HammingDecoder decoder) {
    if (ReceivedSoftBits.empty()) {
        throw std::invalid_argument("ReceivedSoftBits must not be empty");
    }
    harq::HammingEncoder encoder(decoder.r());

    std::vector<uint8_t> hard_bits;
    hard_bits.reserve(ReceivedSoftBits.size());
    for (auto val : ReceivedSoftBits) {
        hard_bits.push_back(val >= 0.0 ? 1 : 0);
    }

    auto candidates = harq::CalculateCandidates(
        hard_bits,
        decoder.r(),
        harq::HAMMING_CODE_DISTANCE,
        ReceivedSoftBits,
        probe_algorithm
    );

    std::vector<std::vector<uint8_t>> valid_info_words;
    std::vector<std::vector<uint8_t>> valid_codewords;

    for (const auto& cw : candidates) {
        auto [info_word, status] = decoder.DecodeWithStatus(cw);
        if (status != HammingDecoder::DecodeStatus::kDetectedDouble) {
            auto reconstructed_cw = encoder.Encode(info_word); // ← нужен метод Encode!
            valid_info_words.push_back(info_word);
            valid_codewords.push_back(reconstructed_cw);
        }
    }

    if (valid_codewords.empty()) {
        auto fallback_info = decoder.Decode(candidates[0]);
        return fallback_info;
    }

    std::size_t best_index = 0;
    double min_distance = std::numeric_limits<double>::max();

    for (std::size_t i = 0; i < valid_codewords.size(); ++i) {
        double dist = 0.0;
        const auto& cw = valid_codewords[i];
        for (std::size_t j = 0; j < cw.size(); ++j) {
            double symbol = cw[j] ? 1.0 : -1.0;
            dist += std::abs(symbol - ReceivedSoftBits[j]);
        }
        if (dist < min_distance) {
            min_distance = dist;
            best_index = i;
        }
    }

    return valid_info_words[best_index];
}

} // namespace harq
