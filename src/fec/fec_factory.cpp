#include "fec/convolutional_codec_aff3ct.hpp"
#include "fec/fec_factory.hpp"

#include "fec/hamming_codec.hpp"

#include <stdexcept>

namespace harq::fec {

bool IsConvolutionalAff3ctAvailable() {
  return true;
}

std::unique_ptr<IFecCodec> CreateCodec(const FecConfig& config) {
  switch (config.codec_type) {
    case CodecType::kHamming:
      return std::make_unique<HammingCodec>(config.hamming_r,
                                            config.hamming_extended);
    case CodecType::kConvolutionalAff3ct:
      return std::make_unique<ConvolutionalCodecAff3ct>(config);
    default:
      throw std::invalid_argument("Unknown codec type.");
  }
}

}  // namespace harq::fec
