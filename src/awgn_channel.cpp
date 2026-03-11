#include "awgn_channel.hpp"

#include <cmath>
#include <random>
#include <stdexcept>

namespace harq {

namespace {
double SnrDbToLinear(double snr_db) {
  return std::pow(10.0, snr_db / 10.0);
}
}  // namespace

AwgnChannel::AwgnChannel(double snr_db, uint32_t seed)
    : snr_db_(snr_db), sigma2_(0.0), sigma_(0.0), seed_(seed) {
  UpdateSigma();
}

void AwgnChannel::SetSnrDb(double snr_db) {
  snr_db_ = snr_db;
  UpdateSigma();
}

std::vector<double> AwgnChannel::AddNoise(
    const std::vector<double>& symbols) {
  std::vector<double> received;
  received.reserve(symbols.size());

  std::mt19937 rng(seed_);
  std::normal_distribution<double> dist(0.0, sigma_);

  for (double symbol : symbols) {
    received.push_back(symbol + dist(rng));
  }

  return received;
}

std::vector<double> AwgnChannel::ComputeLlr(
    const std::vector<double>& received) const {
  std::vector<double> llr;
  llr.reserve(received.size());

  const double scale = (sigma2_ > 0.0) ? (2.0 / sigma2_) : 0.0;
  for (double value : received) {
    llr.push_back(scale * value);
  }

  return llr;
}

std::pair<std::vector<double>, std::vector<double>> AwgnChannel::Transmit(
    const std::vector<double>& symbols) {
  std::vector<double> received = AddNoise(symbols);
  std::vector<double> llr = ComputeLlr(received);
  return {received, llr};
}

void AwgnChannel::UpdateSigma() {
  if (!std::isfinite(snr_db_)) {
    throw std::invalid_argument("SNR must be finite.");
  }

  double snr_linear = SnrDbToLinear(snr_db_);
  if (snr_linear <= 0.0) {
    throw std::invalid_argument("SNR must be positive.");
  }

  sigma2_ = 1.0 / (2 * snr_linear);
  sigma_ = std::sqrt(sigma2_);
}

std::vector<std::complex<double>>
AwgnChannel::TransmitComplex(const std::vector<std::complex<double>>& symbols) {
    std::mt19937 random_generator(seed_);
    std::normal_distribution<double> dist(0.0, sigma_);
    double snr_linear = std::pow(10.0, snr_db_ / 10.0);
    
    sigma_ = std::sqrt(1.0 / (4.0 * snr_linear));
    std::vector<std::complex<double>> noisy_symbols;
    noisy_symbols.reserve(symbols.size());

    std::normal_distribution<double> noise(0.0, sigma_);

    for (const auto& symbol : symbols) {
        double noise_re = noise(random_generator);
        double noise_im = noise(random_generator);
        noisy_symbols.emplace_back(symbol.real() + noise_re, symbol.imag() + noise_im);
    }

    return noisy_symbols;
}

}  // namespace harq
