#include "hamming.hpp"
#include <cassert>
#include <iomanip>
#include <iostream>

void printHeader(const std::string &title) {
  std::cout << "\n" << std::string(70, '=') << "\n";
  std::cout << title << "\n";
  std::cout << std::string(70, '=') << "\n";
}

void testBasicParameters() {
  printHeader("Test 1: Hamming Code Parameters Verification");

  for (int r = 2; r <= 5; ++r) {
    HammingCode code(r);
    int expectedN = (1 << r) - 1;
    int expectedK = expectedN - r;

    std::cout << "r = " << r << ": ";
    std::cout << "n = " << code.getN() << " (expected " << expectedN << "), ";
    std::cout << "k = " << code.getK() << " (expected " << expectedK << "), ";
    std::cout << "d = " << code.getD() << "\n";

    assert(code.getN() == expectedN);
    assert(code.getK() == expectedK);
    assert(code.getD() == 3);
  }

  std::cout << "\n✓ All parameters correct\n";
}

void testEncodeDecode() {
  printHeader("Test 2: Encoding and Decoding without Errors");

  HammingCode code(3);

  std::vector<std::vector<uint8_t>> testVectors = {{0, 0, 0, 0}, {1, 0, 0, 0},
                                                   {0, 1, 0, 0}, {1, 1, 0, 0},
                                                   {1, 0, 1, 0}, {1, 1, 1, 1}};

  for (const auto &data : testVectors) {
    auto encoded = code.encode(data);
    auto decoded = code.decode(encoded);

    std::cout << "Data:     " << HammingCode::vectorToString(data) << "\n";
    std::cout << "Encoded:  " << HammingCode::vectorToString(encoded) << "\n";
    std::cout << "Decoded:  " << HammingCode::vectorToString(decoded) << "\n";

    assert(data == decoded);
    std::cout << "Success\n\n";
  }
}

void testSingleErrorCorrection() {
  printHeader("Test 3: Single Error Correction");

  HammingCode code(3);

  std::vector<uint8_t> data = {1, 0, 1, 1};
  auto encoded = code.encode(data);

  std::cout << "Original data: " << HammingCode::vectorToString(data) << "\n";
  std::cout << "Encoded:       " << HammingCode::vectorToString(encoded)
            << "\n\n";

  for (int errorPos = 0; errorPos < code.getN(); ++errorPos) {
    auto corrupted = encoded;
    HammingCode::injectError(corrupted, errorPos);

    int syndrome = code.calculateSyndrome(corrupted);

    std::cout << "Error at position " << errorPos
              << " (1-based: " << (errorPos + 1) << "):\n";
    std::cout << "  Corrupted: " << HammingCode::vectorToString(corrupted)
              << "\n";
    std::cout << "  Syndrome:  " << syndrome << " (points to position "
              << syndrome << ")\n";

    auto decoded = code.decode(corrupted);
    std::cout << "  Decoded:   " << HammingCode::vectorToString(decoded)
              << "\n";

    assert(data == decoded);
    std::cout << "Error successfully corrected\n\n";
  }
}

void testDoubleErrorDetection() {
  printHeader("Test 4: Double Error Detection");

  HammingCode code(3);

  std::vector<uint8_t> data = {1, 0, 1, 1};
  auto encoded = code.encode(data);

  std::cout << "Original data: " << HammingCode::vectorToString(data) << "\n";
  std::cout << "Encoded:       " << HammingCode::vectorToString(encoded)
            << "\n\n";

  std::cout << "Examples of double errors:\n";

  std::vector<std::pair<int, int>> doubleErrors = {
      {0, 1}, {0, 2}, {1, 3}, {2, 4}, {3, 5}};

  for (const auto &[pos1, pos2] : doubleErrors) {
    auto corrupted = encoded;
    HammingCode::injectErrors(corrupted, {pos1, pos2});

    int syndrome = code.calculateSyndrome(corrupted);
    auto decoded = code.decode(corrupted);

    bool decodingError = (data != decoded);

    std::cout << "Errors at positions " << pos1 << " and " << pos2 << ":\n";
    std::cout << "  Corrupted: " << HammingCode::vectorToString(corrupted)
              << "\n";
    std::cout << "  Syndrome:  " << syndrome << " (incorrect - double error)\n";
    std::cout << "  Decoded:   " << HammingCode::vectorToString(decoded)
              << "\n";
    std::cout << "  Result:    "
              << (decodingError ? "Decoding error (expected)"
                                : "Successful decoding (rare)")
              << "\n\n";
  }

  std::cout
      << "Note: Hamming code detects but does not correct double errors.\n";
  std::cout << "With double error, syndrome points to wrong position.\n";
}

void testMatrices() {
  printHeader("Test 5: Generator and Parity Check Matrices");

  HammingCode code(3);

  std::cout << "Generator matrix G (" << code.getK() << " x " << code.getN()
            << "):\n";
  auto G = code.getGeneratorMatrix();
  for (const auto &row : G) {
    std::cout << "  " << HammingCode::vectorToString(row) << "\n";
  }

  std::cout << "\nParity check matrix H (" << code.getR() << " x "
            << code.getN() << "):\n";
  auto H = code.getParityCheckMatrix();
  for (const auto &row : H) {
    std::cout << "  " << HammingCode::vectorToString(row) << "\n";
  }

  std::cout << "\n✓ Matrices generated\n";
}

int main() {
  std::cout << "HAMMING CODE TESTING\n";

  try {
    testBasicParameters();
    testEncodeDecode();
    testSingleErrorCorrection();
    testDoubleErrorDetection();
    testMatrices();

    printHeader("ALL TESTS PASSED SUCCESSFULLY!");

  } catch (const std::exception &e) {
    std::cerr << "\n Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
