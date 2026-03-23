#include <qpsk.hpp>
#include <stdexcept>

namespace harq {

std::vector<std::complex<double>> QpskModulator::Modulate(const std::vector<uint8_t>& codebits) const {
    if (codebits.size() % 2 != 0) {
        throw std::invalid_argument("Codebits size must be even, got " +
                                    std::to_string(codebits.size()));
    }

    std::vector<std::complex<double>> modulated_symbols;
    modulated_symbols.reserve(codebits.size() / 2);

    for (std::size_t i = 0; i < codebits.size(); i += 2) {
        modulated_symbols.push_back(MapBitsToSymbol(codebits[i], codebits[i + 1]));
    }

    return modulated_symbols;
}

std::vector<std::complex<double>> QpskModulate(const std::vector<uint8_t>& bits) {
  return QpskModulator{}.Modulate(bits);
}

std::complex<double> QpskModulator::MapBitsToSymbol(uint8_t msb, uint8_t lsb) const {
    double re = (msb == 1) ? 1.0 : -1.0;
    double im = (lsb == 1) ? 1.0 : -1.0;

    return std::complex<double>(re, im) / std::sqrt(2.0);
}

std::vector<double> QpskDemodulator::Demodulate(const std::vector<std::complex<double>>& symbols,
                                                double snr_db) const {
    std::vector<double> llr_values;
    llr_values.reserve(symbols.size() * 2);
    double snr_linear = std::pow(10.0, snr_db / 10.0);

    for (const auto& symbol : symbols) {
        double snr_linear = std::pow(10.0, snr_db / 10.0);
        double scale = 4.0 * snr_linear;
        llr_values.push_back(scale * symbol.real());
        llr_values.push_back(scale * symbol.imag());
    }

    return llr_values;
}


std::vector<double> QpskDemodulate(const std::vector<std::complex<double>>& symbols, double snr_db) {
  return QpskDemodulator{}.Demodulate(symbols, snr_db);
}

std::vector<uint8_t> QpskDemodulator::DemodulateHard(const std::vector<std::complex<double>>& symbols,
                                                double snr_db) const {
    std::vector<uint8_t> llr_values;
    llr_values.reserve(symbols.size() * 2);
    double snr_linear = std::pow(10.0, snr_db / 10.0);

    for (const auto& symbol : symbols) {
        llr_values.push_back(symbol.real() >= 0 ? 1 : 0);
        llr_values.push_back(symbol.imag() >= 0 ? 1 : 0);
    }

    return llr_values;
}

std::vector<uint8_t> QpskDemodulateHard(const std::vector<std::complex<double>>& symbols, double snr_db) {
  return QpskDemodulator{}.DemodulateHard(symbols, snr_db);
}

}  // namespace harq
