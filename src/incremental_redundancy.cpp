#include "incremental_redundancy.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>

namespace harq {
namespace {

std::size_t NormalizeRv(int rv, std::size_t redundancy_versions) {
    if (redundancy_versions == 0) {
        throw std::invalid_argument("redundancy_versions must be positive.");
    }
    if (redundancy_versions >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("redundancy_versions is too large.");
    }

    const int rv_count = static_cast<int>(redundancy_versions);
    int normalized = rv % rv_count;
    if (normalized < 0) {
        normalized += rv_count;
    }
    return static_cast<std::size_t>(normalized);
}

void ValidatePermutation(const std::vector<std::size_t>& perm,
                         std::size_t expected_size) {
    if (expected_size == 0) {
        throw std::invalid_argument("IR buffer size must be positive.");
    }
    if (perm.size() != expected_size) {
        throw std::invalid_argument(
            "IR permutation size must match the mother codeword size.");
    }

    std::vector<bool> seen(expected_size, false);
    for (std::size_t idx : perm) {
        if (idx >= expected_size || seen[idx]) {
            throw std::invalid_argument(
                "IR permutation must contain each codeword position exactly once.");
        }
        seen[idx] = true;
    }
}

std::size_t MakeEven(std::size_t value) {
    return value + (value % 2);
}

}  // namespace

std::vector<std::size_t> make_perm(std::size_t size, std::uint32_t seed) {
    if (size == 0) {
        throw std::invalid_argument("IR permutation size must be positive.");
    }

    std::vector<std::size_t> perm = make_identity_perm(size);

    std::mt19937 rng(seed);
    std::shuffle(perm.begin(), perm.end(), rng);

    return perm;
}

std::vector<std::size_t> make_identity_perm(std::size_t size) {
    if (size == 0) {
        throw std::invalid_argument("IR permutation size must be positive.");
    }

    std::vector<std::size_t> perm(size);
    std::iota(perm.begin(), perm.end(), std::size_t{0});
    return perm;
}

std::size_t DefaultIrTransmissionSize(std::size_t buffer_size,
                                      std::size_t redundancy_versions) {
    if (buffer_size == 0) {
        throw std::invalid_argument("IR buffer size must be positive.");
    }
    if (redundancy_versions == 0) {
        throw std::invalid_argument("redundancy_versions must be positive.");
    }

    return MakeEven(1 + (buffer_size - 1) / redundancy_versions);
}

std::size_t RedundancyVersionOffset(std::size_t buffer_size,
                                    int rv,
                                    std::size_t redundancy_versions) {
    return RedundancyVersionOffset(
        buffer_size, rv,
        DefaultIrTransmissionSize(buffer_size, redundancy_versions),
        redundancy_versions);
}

std::size_t RedundancyVersionOffset(std::size_t buffer_size,
                                    int rv,
                                    std::size_t transmitted_bits,
                                    std::size_t redundancy_versions) {
    if (buffer_size == 0) {
        throw std::invalid_argument("IR buffer size must be positive.");
    }
    if (transmitted_bits == 0) {
        throw std::invalid_argument("transmitted_bits must be positive.");
    }
    transmitted_bits = MakeEven(transmitted_bits);

    const std::size_t normalized_rv = NormalizeRv(rv, redundancy_versions);
    return (normalized_rv * (transmitted_bits % buffer_size)) % buffer_size;
}

std::vector<uint8_t> RateMatch(const std::vector<uint8_t>& buffer,
                               int rv,
                               std::size_t transmitted_bits,
                               const std::vector<std::size_t>& perm,
                               std::size_t redundancy_versions) {
    ValidatePermutation(perm, buffer.size());
    if (transmitted_bits == 0) {
        throw std::invalid_argument("transmitted_bits must be positive.");
    }
    transmitted_bits = MakeEven(transmitted_bits);

    const std::size_t buffer_size = buffer.size();
    const std::size_t offset =
        RedundancyVersionOffset(buffer_size, rv, transmitted_bits,
                                redundancy_versions);

    std::vector<uint8_t> out;
    out.reserve(transmitted_bits);

    for (std::size_t i = 0; i < transmitted_bits; ++i) {
        const std::size_t perm_pos = (offset + i) % buffer_size;
        out.push_back(buffer[perm[perm_pos]]);
    }

    return out;
}

std::vector<uint8_t> RateMatch(const std::vector<uint8_t>& buffer,
                               int rv,
                               const std::vector<std::size_t>& perm,
                               std::size_t redundancy_versions) {
    return RateMatch(buffer, rv,
                     DefaultIrTransmissionSize(buffer.size(),
                                               redundancy_versions),
                     perm, redundancy_versions);
}

std::vector<double> RateDematch(const std::vector<double>& received_llr,
                                int rv,
                                std::size_t buffer_size,
                                const std::vector<std::size_t>& perm,
                                std::size_t redundancy_versions) {
    ValidatePermutation(perm, buffer_size);
    if (received_llr.empty()) {
        throw std::invalid_argument("received_llr must not be empty.");
    }

    const std::size_t offset =
        RedundancyVersionOffset(buffer_size, rv, received_llr.size(),
                                redundancy_versions);
    std::vector<double> llr_buffer(buffer_size, 0.0);

    for (std::size_t i = 0; i < received_llr.size(); ++i) {
        const std::size_t perm_pos = (offset + i) % buffer_size;
        llr_buffer[perm[perm_pos]] += received_llr[i];
    }

    return llr_buffer;
}

std::vector<double> CombineIncrementalRedundancySoft(
    const std::vector<std::vector<double>>& history,
    std::size_t buffer_size,
    const std::vector<std::size_t>& perm,
    std::size_t redundancy_versions) {
    ValidatePermutation(perm, buffer_size);
    if (history.empty()) {
        throw std::invalid_argument("IR history must not be empty.");
    }

    std::vector<double> combined(buffer_size, 0.0);
    for (std::size_t attempt = 0; attempt < history.size(); ++attempt) {
        const auto dematched =
            RateDematch(history[attempt], static_cast<int>(attempt),
                        buffer_size, perm, redundancy_versions);

        for (std::size_t i = 0; i < buffer_size; ++i) {
            combined[i] += dematched[i];
        }
    }

    return combined;
}

std::vector<uint8_t> DecodeIncrementalRedundancy(
    const fec::IFecCodec& codec,
    const std::vector<std::vector<double>>& history,
    std::size_t buffer_size,
    const std::vector<std::size_t>& perm,
    std::size_t redundancy_versions) {
    if (buffer_size != static_cast<std::size_t>(codec.output_bits_per_frame())) {
        throw std::invalid_argument(
            "IR buffer size must match codec output_bits_per_frame().");
    }

    return codec.DecodeSoft(CombineIncrementalRedundancySoft(
        history, buffer_size, perm, redundancy_versions));
}

}  // namespace harq