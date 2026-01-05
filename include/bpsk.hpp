#pragma once

#include <cstdint>
#include <vector>

namespace harq {

class BpskModulator {
 public:
  std::vector<double> Modulate(const std::vector<uint8_t>& bits) const;
};

std::vector<double> BpskModulate(const std::vector<uint8_t>& bits);

}  // namespace harq
