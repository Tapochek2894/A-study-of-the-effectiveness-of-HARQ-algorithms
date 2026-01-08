#ifndef HAMMING_H
#define HAMMING_H

#include <cstdint>
#include <string>
#include <vector>

class HammingCode {
public:
  explicit HammingCode(int r);

  int getN() const { return n; }
  int getK() const { return k; }
  int getD() const { return d; }
  int getR() const { return r; }

  std::vector<uint8_t> encode(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t> decode(const std::vector<uint8_t> &received) const;
  int calculateSyndrome(const std::vector<uint8_t> &received) const;
  bool correctSingleError(std::vector<uint8_t> &word) const;
  bool detectDoubleError(const std::vector<uint8_t> &word) const;

  std::vector<std::vector<uint8_t>> getGeneratorMatrix() const;
  std::vector<std::vector<uint8_t>> getParityCheckMatrix() const;

  static void injectError(std::vector<uint8_t> &word, int errorPos);
  static void injectErrors(std::vector<uint8_t> &word,
                           const std::vector<int> &errorPositions);
  static std::string vectorToString(const std::vector<uint8_t> &vec);

private:
  int r;
  int n;
  int k;
  int d;

  static bool isParityPosition(int pos);
  uint8_t calculateParityBit(const std::vector<uint8_t> &word,
                             int parityPos) const;
};

#endif // HAMMING_H
