#pragma once

#include "fec/ifec_codec.hpp"

#include <vector>
#include <cstddef>
#include <cstdint>

namespace harq
{

constexpr std::size_t kDefaultIrRedundancyVersions = 4;

std::vector<std::size_t> make_perm(std::size_t size, std::uint32_t seed);

std::vector<std::size_t> make_identity_perm(std::size_t size);

std::size_t DefaultIrTransmissionSize(
    std::size_t buffer_size,
    std::size_t redundancy_versions = kDefaultIrRedundancyVersions);

std::size_t RedundancyVersionOffset(
    std::size_t buffer_size,
    int rv,
    std::size_t redundancy_versions = kDefaultIrRedundancyVersions);

std::size_t RedundancyVersionOffset(
    std::size_t buffer_size,
    int rv,
    std::size_t transmitted_bits,
    std::size_t redundancy_versions);

std::vector<uint8_t> RateMatch(
    const std::vector<uint8_t>& buffer,
    int rv,
    std::size_t transmitted_bits,
    const std::vector<std::size_t>& perm,
    std::size_t redundancy_versions = kDefaultIrRedundancyVersions);

std::vector<uint8_t> RateMatch(
    const std::vector<uint8_t>& buffer,
    int rv,
    const std::vector<std::size_t>& perm,
    std::size_t redundancy_versions = kDefaultIrRedundancyVersions);

std::vector<double> RateDematch(
    const std::vector<double>& received_llr,
    int rv,
    std::size_t buffer_size,
    const std::vector<std::size_t>& perm,
    std::size_t redundancy_versions = kDefaultIrRedundancyVersions);

std::vector<double> CombineIncrementalRedundancySoft(
    const std::vector<std::vector<double>>& history,
    std::size_t buffer_size,
    const std::vector<std::size_t>& perm,
    std::size_t redundancy_versions = kDefaultIrRedundancyVersions);

std::vector<uint8_t> DecodeIncrementalRedundancy(
    const fec::IFecCodec& codec,
    const std::vector<std::vector<double>>& history,
    std::size_t buffer_size,
    const std::vector<std::size_t>& perm,
    std::size_t redundancy_versions = kDefaultIrRedundancyVersions);

} // namespace harq