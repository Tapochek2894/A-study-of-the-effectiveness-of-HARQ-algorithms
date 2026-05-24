#include "rayleigh_channel.hpp"

#include <cmath>
#include <random>
#include <stdexcept>

namespace harq {

namespace {

constexpr double kHalf = 0.5;
constexpr double kTwoPi = 6.283185307179586476925286766559;

double SnrDbToLinear(double snr_db) {
  return std::pow(10.0, snr_db / 10.0);
}

double DrawRayleighAmplitude(std::mt19937& rng) {
  // x, y ~ N(0, 1/2)  ->  μ = sqrt(x² + y²) ~ Rayleigh, E[μ²] = 1.
  std::normal_distribution<double> g(0.0, std::sqrt(kHalf));
  const double x = g(rng);
  const double y = g(rng);
  return std::sqrt(x * x + y * y);
}

}  // namespace

RayleighChannel::RayleighChannel(double snr_db, uint32_t seed, int block_size)
    : snr_db_(snr_db), sigma2_(0.0), sigma_(0.0), seed_(seed),
      block_size_(block_size), rng_(seed) {
  if (block_size_ < 1) {
    throw std::invalid_argument("block_size must be >= 1.");
  }
  UpdateSigma();
}

void RayleighChannel::SetSnrDb(double snr_db) {
  snr_db_ = snr_db;
  UpdateSigma();
}

void RayleighChannel::SetBlockSize(int block_size) {
  if (block_size < 1) {
    throw std::invalid_argument("block_size must be >= 1.");
  }
  block_size_ = block_size;
}

std::vector<double> RayleighChannel::AddNoise(
    const std::vector<double>& symbols) {
  std::vector<double> received;
  received.reserve(symbols.size());
  last_fading_.assign(symbols.size(), 0.0);

  std::normal_distribution<double> noise(0.0, sigma_);

  double mu = 0.0;
  for (std::size_t i = 0; i < symbols.size(); ++i) {
    if (i % static_cast<std::size_t>(block_size_) == 0) {
      mu = DrawRayleighAmplitude(rng_);
    }
    last_fading_[i] = mu;
    received.push_back(mu * symbols[i] + noise(rng_));
  }

  return received;
}

std::vector<double> RayleighChannel::ComputeLlr(
    const std::vector<double>& received,
    const std::vector<double>& fading) const {
  if (received.size() != fading.size()) {
    throw std::invalid_argument(
        "ComputeLlr: received/fading size mismatch.");
  }
  std::vector<double> llr;
  llr.reserve(received.size());

  const double scale = (sigma2_ > 0.0) ? (2.0 / sigma2_) : 0.0;
  for (std::size_t i = 0; i < received.size(); ++i) {
    llr.push_back(scale * fading[i] * received[i]);
  }
  return llr;
}

std::pair<std::vector<double>, std::vector<double>>
RayleighChannel::Transmit(const std::vector<double>& symbols) {
  std::vector<double> received = AddNoise(symbols);
  std::vector<double> llr = ComputeLlr(received, last_fading_);
  return {received, llr};
}

std::vector<std::complex<double>> RayleighChannel::TransmitComplex(
    const std::vector<std::complex<double>>& symbols) {
  std::vector<std::complex<double>> out;
  out.reserve(symbols.size());
  last_fading_.assign(symbols.size(), 0.0);

  // Для комплексного канала шум распределяется поровну между I и Q:
  // дисперсия каждой компоненты = N0/2 / 2 = 1/(4·SNR).
  const double snr_linear = SnrDbToLinear(snr_db_);
  const double sigma_iq = std::sqrt(1.0 / (4.0 * snr_linear));
  std::normal_distribution<double> noise(0.0, sigma_iq);
  std::uniform_real_distribution<double> phase(0.0, kTwoPi);

  // Когерентный приёмник: после канала r = h·s + n; зная h = μ·exp(jθ),
  // приёмник домножает на exp(−jθ), получая r' = μ·s + n'. Дисперсия n'
  // равна дисперсии n (унитарное преобразование). Возвращаем r' — это
  // стандартная модель «coherent Rayleigh» из учебников.
  double mu = 0.0;
  double theta = 0.0;
  for (std::size_t i = 0; i < symbols.size(); ++i) {
    if (i % static_cast<std::size_t>(block_size_) == 0) {
      mu = DrawRayleighAmplitude(rng_);
      theta = phase(rng_);
    }
    last_fading_[i] = mu;
    const std::complex<double> h(mu * std::cos(theta), mu * std::sin(theta));
    const std::complex<double> n(noise(rng_), noise(rng_));
    const std::complex<double> r = h * symbols[i] + n;
    // Компенсация фазы: умножение на exp(−jθ).
    const std::complex<double> exp_neg_jtheta(std::cos(theta), -std::sin(theta));
    out.push_back(r * exp_neg_jtheta);
  }

  return out;
}

void RayleighChannel::UpdateSigma() {
  if (!std::isfinite(snr_db_)) {
    throw std::invalid_argument("SNR must be finite.");
  }
  const double snr_linear = SnrDbToLinear(snr_db_);
  if (snr_linear <= 0.0) {
    throw std::invalid_argument("SNR must be positive.");
  }
  // Та же нормировка, что и в AwgnChannel: E[symbol²]=1, E[μ²]=1,
  // sigma² = 1/(2·SNR) для вещественного канала.
  sigma2_ = 1.0 / (2.0 * snr_linear);
  sigma_ = std::sqrt(sigma2_);
}

}  // namespace harq
