#include "hamming.hpp"
#include <sstream>
#include <stdexcept>

HammingCode::HammingCode(int r) : r(r), d(3) {
  if (r < 2) {
    throw std::invalid_argument("The number of check bits must be >= 2");
  }

  n = (1 << r) - 1;
  k = n - r;
}

bool HammingCode::isParityPosition(int pos) {
  return pos > 0 && (pos & (pos - 1)) == 0;
}

uint8_t HammingCode::calculateParityBit(const std::vector<uint8_t> &word,
                                        int parityPos) const {
  uint8_t parity = 0;

  for (int i = 1; i <= n; ++i) {
    if (i & parityPos) {
      parity ^= word[i - 1];
    }
  }

  return parity;
}

std::vector<uint8_t>
HammingCode::encode(const std::vector<uint8_t> &data) const {
  if (data.size() != static_cast<size_t>(k)) {
    throw std::invalid_argument("Wrong length for the information word");
  }

  for (auto bit : data) {
    if (bit != 0 && bit != 1) {
      throw std::invalid_argument("Data should contains only 0 and 1");
    }
  }

  std::vector<uint8_t> codeword(n, 0);

  int dataIdx = 0;
  for (int i = 1; i <= n; ++i) {
    if (!isParityPosition(i)) {
      codeword[i - 1] = data[dataIdx++];
    }
  }

  for (int i = 1; i <= n; ++i) {
    if (isParityPosition(i)) {
      codeword[i - 1] = calculateParityBit(codeword, i);
    }
  }

  return codeword;
}

int HammingCode::calculateSyndrome(const std::vector<uint8_t> &received) const {
  if (received.size() != static_cast<size_t>(n)) {
    throw std::invalid_argument("Incorrect length of the accepted word");
  }

  int syndrome = 0;

  for (int i = 0; i < r; ++i) {
    int parityPos = 1 << i;
    uint8_t parity = calculateParityBit(received, parityPos);

    if (parity != 0) {
      syndrome |= parityPos;
    }
  }

  return syndrome;
}

bool HammingCode::correctSingleError(std::vector<uint8_t> &word) const {
  int syndrome = calculateSyndrome(word);

  if (syndrome == 0) {
    return false;
  }

  if (syndrome >= 1 && syndrome <= n) {
    word[syndrome - 1] ^= 1;
    return true;
  }

  return false;
}

bool HammingCode::detectDoubleError(const std::vector<uint8_t> &word) const {
  int syndrome = calculateSyndrome(word);

  if (syndrome == 0) {
    return false;
  }

  return false;
}

std::vector<uint8_t>
HammingCode::decode(const std::vector<uint8_t> &received) const {
  if (received.size() != static_cast<size_t>(n)) {
    throw std::invalid_argument("Incorrect length of the accepted word");
  }

  std::vector<uint8_t> corrected = received;

  correctSingleError(corrected);

  std::vector<uint8_t> decoded;
  decoded.reserve(k);

  for (int i = 1; i <= n; ++i) {
    if (!isParityPosition(i)) {
      decoded.push_back(corrected[i - 1]);
    }
  }

  return decoded;
}

void HammingCode::injectError(std::vector<uint8_t> &word, int errorPos) {
  if (errorPos >= 0 && errorPos < static_cast<int>(word.size())) {
    word[errorPos] ^= 1;
  }
}

void HammingCode::injectErrors(std::vector<uint8_t> &word,
                               const std::vector<int> &errorPositions) {
  for (int pos : errorPositions) {
    injectError(word, pos);
  }
}

std::string HammingCode::vectorToString(const std::vector<uint8_t> &vec) {
  std::ostringstream oss;
  for (size_t i = 0; i < vec.size(); ++i) {
    oss << static_cast<int>(vec[i]);
    if (i < vec.size() - 1)
      oss << " ";
  }
  return oss.str();
}

std::vector<std::vector<uint8_t>> HammingCode::getGeneratorMatrix() const {
  std::vector<std::vector<uint8_t>> G(k, std::vector<uint8_t>(n, 0));

  int row = 0;
  for (int i = 1; i <= n; ++i) {
    if (!isParityPosition(i)) {
      std::vector<uint8_t> infoWord(k, 0);
      infoWord[row] = 1;

      std::vector<uint8_t> codeword = encode(infoWord);
      G[row] = codeword;
      row++;
    }
  }

  return G;
}

std::vector<std::vector<uint8_t>> HammingCode::getParityCheckMatrix() const {
  std::vector<std::vector<uint8_t>> H(r, std::vector<uint8_t>(n, 0));

  for (int col = 1; col <= n; ++col) {
    for (int row = 0; row < r; ++row) {
      if (col & (1 << row)) {
        H[row][col - 1] = 1;
      }
    }
  }

  return H;
}
