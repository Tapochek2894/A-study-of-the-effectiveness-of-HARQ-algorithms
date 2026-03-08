#pragma once

#include <cstdint>
#include <vector>

namespace harq::fec {

class IFecCodec {
 public:
  virtual ~IFecCodec() = default;

  virtual int input_bits_per_frame() const = 0;
  virtual int output_bits_per_frame() const = 0;

  virtual std::vector<uint8_t> Encode(
      const std::vector<uint8_t>& info_bits) const = 0;
  virtual std::vector<uint8_t> DecodeHard(
      const std::vector<uint8_t>& codeword_bits) const = 0;
  virtual std::vector<uint8_t> DecodeSoft(
      const std::vector<double>& soft_bits) const = 0;
};

}  // namespace harq::fec
