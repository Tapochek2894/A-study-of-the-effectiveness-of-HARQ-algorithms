#include "bpsk.hpp"

#include <stdexcept>

namespace harq {

std::vector<double> BpskModulator::Modulate(
    const std::vector<uint8_t>& bits) const {
  std::vector<double> symbols;
  symbols.reserve(bits.size());

  for (uint8_t bit : bits) {
    if (bit == 0) {
      symbols.push_back(-1.0);
    } else if (bit == 1) {
      symbols.push_back(1.0);
    } else {
      throw std::invalid_argument("BPSK modulator expects bits 0 or 1.");
    }
  }

  return symbols;
}

std::vector<double> BpskModulate(const std::vector<uint8_t>& bits) {
  return BpskModulator{}.Modulate(bits);
}

std::vector<uint8_t> BpskDemodulator::Demodulate(
    const std::vector<double>& symbols) const {
  std::vector<uint8_t> bits;
  bits.reserve(symbols.size());

  for (double symbol : symbols) {
    bits.push_back(symbol >= 0.0 ? 1 : 0);
  }

  return bits;
}

std::vector<uint8_t> BpskDemodulate(const std::vector<double>& symbols) {
  return BpskDemodulator{}.Demodulate(symbols);
}

}  // namespace harq
