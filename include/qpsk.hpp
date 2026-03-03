#pragma once

#include <cstdint>
#include <vector>
#include <complex>

namespace harq {

class QpskModulator {
public:
    std::vector<std::complex<double>> Modulate(const std::vector<uint8_t>& bits) const;
private:
    std::complex<double> MapBitsToSymbol(uint8_t msb, uint8_t lsb) const;
};

std::vector<std::complex<double>> QpskModulate(const std::vector<uint8_t>& bits);

class QpskDemodulator {
public:
    std::vector<double> Demodulate(const std::vector<std::complex<double>>& symbols,
                                                double snr_db) const;
private:
    std::pair<double, double> ComputeLlr(const std::complex<double>& symbol,
                                                      double snr_linear) const;
};

std::vector<double> QpskDemodulate(const std::vector<double>& symbols, double snr_db);

}  // namespace harq
