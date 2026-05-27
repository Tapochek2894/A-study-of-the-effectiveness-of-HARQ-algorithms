#include "rayleigh_channel.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

double RayleighTheoreticalBpskBer(double snr_db) {
  const double gamma = std::pow(10.0, snr_db / 10.0);
  return 0.5 * (1.0 - std::sqrt(gamma / (1.0 + gamma)));
}

}  // namespace

TEST(RayleighChannelTest, RejectsInvalidParameters) {
  EXPECT_THROW(harq::RayleighChannel(0.0, 1u, 0), std::invalid_argument);
  EXPECT_THROW(harq::RayleighChannel(std::nan("")), std::invalid_argument);
}

TEST(RayleighChannelTest, AmplitudesHaveUnitSecondMoment) {
  harq::RayleighChannel channel(30.0, 42u, 1);
  const std::vector<double> symbols(20000, 1.0);
  channel.AddNoise(symbols);
  const auto& fading = channel.last_fading_amplitudes();
  ASSERT_EQ(fading.size(), symbols.size());

  double sum_sq = 0.0;
  double mean = 0.0;
  for (double mu : fading) {
    EXPECT_GE(mu, 0.0);
    sum_sq += mu * mu;
    mean += mu;
  }
  const double e_sq = sum_sq / fading.size();
  const double e_mu = mean / fading.size();
  // E[μ²] = 1, E[μ] = √(π/4) ≈ 0.8862 для Релея с σ=1/√2.
  EXPECT_NEAR(e_sq, 1.0, 0.05);
  EXPECT_NEAR(e_mu, std::sqrt(kPi / 4.0), 0.03);
}

TEST(RayleighChannelTest, BlockFadingHoldsAmplitudeConstant) {
  const int block = 100;
  harq::RayleighChannel channel(10.0, 7u, block);
  std::vector<double> symbols(block * 5, 1.0);
  channel.AddNoise(symbols);
  const auto& fading = channel.last_fading_amplitudes();
  for (std::size_t i = 0; i < fading.size(); ++i) {
    const std::size_t block_start =
        (i / static_cast<std::size_t>(block)) * static_cast<std::size_t>(block);
    EXPECT_DOUBLE_EQ(fading[i], fading[block_start]);
  }
}

TEST(RayleighChannelTest, UncodedBerMatchesTheoryWithinTolerance) {
  // При большом числе бит и SNR = 10 дБ теоретический BER ≈ 2.33·10⁻².
  const double snr_db = 10.0;
  const std::size_t n_bits = 200000;
  harq::RayleighChannel channel(snr_db, 12345u, 1);

  std::vector<double> symbols(n_bits, 1.0);
  // Передаём «все единицы»: после канала знак ν = μ·1 + n всегда > 0
  // означает успешный приём. BER = доля случаев, когда μ + n < 0.
  std::vector<double> received = channel.AddNoise(symbols);

  std::size_t errors = 0;
  for (double v : received) {
    if (v < 0.0) {
      ++errors;
    }
  }
  const double ber = static_cast<double>(errors) / static_cast<double>(n_bits);
  const double ber_theory = RayleighTheoreticalBpskBer(snr_db);
  EXPECT_NEAR(ber, ber_theory, 5e-3);
}

TEST(RayleighChannelTest, LlrSignMatchesReceivedSign) {
  harq::RayleighChannel channel(5.0, 99u);
  std::vector<double> symbols = {1.0, -1.0, 1.0, -1.0, 1.0};
  auto [received, llr] = channel.Transmit(symbols);
  ASSERT_EQ(llr.size(), received.size());
  const auto& fading = channel.last_fading_amplitudes();
  for (std::size_t i = 0; i < received.size(); ++i) {
    if (fading[i] > 0.0) {
      EXPECT_EQ(std::signbit(llr[i]), std::signbit(received[i]));
    }
  }
}
