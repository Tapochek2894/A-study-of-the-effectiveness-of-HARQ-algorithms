#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace harq {

// Канал АБГШ (AWGN) для BPSK при единичной мощности символов.
class AwgnChannel {
 public:
  explicit AwgnChannel(double snr_db, uint32_t seed = 5489u);

  // Обновляет SNR и внутреннюю дисперсию шума.
  void SetSnrDb(double snr_db);

  // Возвращает символы с добавленным гауссовским шумом.
  std::vector<double> AddNoise(const std::vector<double>& symbols);

  // Вычисляет LLR для принятых символов при текущем SNR.
  std::vector<double> ComputeLlr(const std::vector<double>& received) const;

  // Полная передача: шум + LLR.
  std::pair<std::vector<double>, std::vector<double>> Transmit(
      const std::vector<double>& symbols);

 private:
  void UpdateSigma();

  double snr_db_;
  double sigma2_;
  double sigma_;
  uint32_t seed_;
};

}  // namespace harq
