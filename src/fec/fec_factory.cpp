#include "fec/fec_factory.hpp"

#include "fec/hamming_codec.hpp"

#include <stdexcept>

namespace harq::fec {

std::unique_ptr<IFecCodec> CreateCodec(const FecConfig& config) {
  switch (config.codec_type) {
    case CodecType::kHamming:
      return std::make_unique<HammingCodec>(config.hamming_r,
                                            config.hamming_extended);
    case CodecType::kConvolutionalAff3ct:
      throw std::invalid_argument(
          "Convolutional AFF3CT codec is not available in this build yet.");
    default:
      throw std::invalid_argument("Unknown codec type.");
  }
}

}  // namespace harq::fec
