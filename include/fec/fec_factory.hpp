#pragma once

#include "fec/fec_config.hpp"
#include "fec/ifec_codec.hpp"

#include <memory>

namespace harq::fec {

std::unique_ptr<IFecCodec> CreateCodec(const FecConfig& config);
bool IsConvolutionalAff3ctAvailable();

}  // namespace harq::fec
