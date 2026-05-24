#pragma once

#include <complex>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace harq {

// Канал с релеевскими замираниями + АБГШ.
//
// Принимаемый сигнал (когерентный приём, фаза θ скомпенсирована):
//   r = μ·s + n,
// где μ = √(x² + y²), x,y ∼ N(0, 1/2) независимы → μ ∼ Rayleigh с E[μ²] = 1
// (Трофимов А. Н., «Основы теории цифровой связи», §3.6–3.7).
//
// block_size управляет когерентностью замираний:
//   1                — fast fading: μ независим на каждый символ.
//   N (длина кадра)  — flat (block) fading: μ постоянен в пределах блока.
class RayleighChannel {
 public:
  explicit RayleighChannel(double snr_db, uint32_t seed = 5489u,
                           int block_size = 1);

  void SetSnrDb(double snr_db);
  void SetBlockSize(int block_size);

  double snr_db() const { return snr_db_; }
  int block_size() const { return block_size_; }

  // Прогоняет вещественные BPSK-символы через канал и возвращает r = μ·s + n.
  // Коэффициенты замираний μ для каждого символа сохраняются и доступны через
  // last_fading_amplitudes().
  std::vector<double> AddNoise(const std::vector<double>& symbols);

  // Коэффициенты замираний μ, использованные в последнем вызове AddNoise/Transmit.
  const std::vector<double>& last_fading_amplitudes() const {
    return last_fading_;
  }

  // LLR для когерентного приёма при известных μ: L = 2·μ·r / σ².
  // Длина fading должна совпадать с длиной received.
  std::vector<double> ComputeLlr(const std::vector<double>& received,
                                 const std::vector<double>& fading) const;

  // Полная передача: r и LLR (по последним μ).
  std::pair<std::vector<double>, std::vector<double>> Transmit(
      const std::vector<double>& symbols);

  // Комплексный вариант: r = μ·exp(jθ)·s + n, θ ∼ U[0, 2π).
  // Коэффициенты μ доступны через last_fading_amplitudes().
  std::vector<std::complex<double>> TransmitComplex(
      const std::vector<std::complex<double>>& symbols);

 private:
  void UpdateSigma();

  double snr_db_;
  double sigma2_;
  double sigma_;
  uint32_t seed_;
  int block_size_;
  std::mt19937 rng_;
  std::vector<double> last_fading_;
};

}  // namespace harq
