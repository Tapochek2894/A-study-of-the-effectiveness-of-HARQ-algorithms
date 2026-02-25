#include "chase_combining.hpp"
#include <cassert>
#include <iostream>
#include <map>
#include <bpsk.hpp>
#include <limits>

namespace harq {

struct candidate {
  std::size_t index;
  double weight;
};


std::vector<uint8_t> ChaseCombiningHammingNoCRC(harq::ProbeAlgorithm ProbeAlgorithm,
    harq::HammingDecoder decoder, std::vector<std::vector<double>> soft_bits) {
    
    auto last_msg = BpskDemodulate(soft_bits[soft_bits.size() - 1]);
    std::vector<std::vector<uint8_t>> hard_msgs;
    for (auto & i : soft_bits) {
      hard_msgs.push_back(BpskDemodulate(i));
    }

    int d;  
    if (soft_bits[0].size() % 2 == 0) {
      d = 4;
    } else {
      d = 3;
    }
  
    auto candidates = harq::CalculateCandidates(last_msg, decoder.r(), d, soft_bits[0], ProbeAlgorithm);
    int minimal_idx = 0;
    double minimal_weight = std::numeric_limits<double>::max();

    std::vector<candidate> metrics;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      candidate current {i, 0.0};
      for (std::size_t j = 0; j < soft_bits.size(); ++j) {
        current.weight += ComputeSoftDistance(candidates[i], soft_bits[j]);
      }
      if (current.weight < minimal_weight) {
        minimal_weight = current.weight;
        minimal_idx = current.index;
      }
      metrics.push_back(current);
    }
  
    return candidates[minimal_idx];
}
} // namespace harq
