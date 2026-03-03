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
    double re = (msb == 0) ? 1.0 : -1.0;
    double im = (lsb == 0) ? 1.0 : -1.0;

    return std::complex<double>(re, im) / std::sqrt(2.0);
}

std::vector<double> QpskDemodulator::Demodulate(const std::vector<std::complex<double>>& symbols,
                                                double snr_db) const {
    std::vector<double> llr_values;
    llr_values.reserve(symbols.size() * 2);
    double snr_linear = std::pow(10.0, snr_db / 10.0);

    for (const auto& symbol : symbols) {
        auto [llr_msb, llr_lsb] = ComputeLlr(symbol, snr_linear);
        llr_values.push_back(llr_msb);
        llr_values.push_back(llr_lsb);
    }

    return llr_values;
}


std::vector<double> QpskDemodulate(const std::vector<std::complex<double>>& symbols, double snr_db) {
  return QpskDemodulator{}.Demodulate(symbols, snr_db);
}

std::pair<double, double> QpskDemodulator::ComputeLlr(const std::complex<double>& symbol,
                                                      double snr_linear) const {
    double scale = 2.0 * std::sqrt(2.0) * snr_linear;

    double llr_msb = -scale * symbol.real();
    double llr_lsb = -scale * symbol.imag();

    return {llr_msb, llr_lsb};
}

}  // namespace harq
