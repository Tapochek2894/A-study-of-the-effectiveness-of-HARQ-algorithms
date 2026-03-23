#include "qpsk.hpp"
#include <cmath>
#include <gtest/gtest.h>

constexpr double EPSILON = 1e-4;


TEST(QpskModulatorTest, EvenBitsRequired) {
    harq::QpskModulator modulator;

    EXPECT_THROW(modulator.Modulate({0}), std::invalid_argument);
    EXPECT_THROW(modulator.Modulate({0, 0, 0}), std::invalid_argument);

    EXPECT_NO_THROW(modulator.Modulate({0, 0}));
    EXPECT_NO_THROW(modulator.Modulate({0, 0, 0, 0}));
}

TEST(QpskModulatorTest, QpskMapping) {
    harq::QpskModulator modulator;

    auto symbols = modulator.Modulate({0, 0});
    EXPECT_NEAR(symbols[0].real(), -1.0 / std::sqrt(2.0), EPSILON);
    EXPECT_NEAR(symbols[0].imag(), -1.0 / std::sqrt(2.0), EPSILON);

    symbols = modulator.Modulate({1, 1});
    EXPECT_NEAR(symbols[0].real(), 1.0 / std::sqrt(2.0), EPSILON);
    EXPECT_NEAR(symbols[0].imag(), 1.0 / std::sqrt(2.0), EPSILON);

    symbols = modulator.Modulate({0, 1});
    EXPECT_NEAR(symbols[0].real(), -1.0 / std::sqrt(2.0), EPSILON);
    EXPECT_NEAR(symbols[0].imag(), 1.0 / std::sqrt(2.0), EPSILON);
}

TEST(QpskModulatorTest, OutputCount) {
    harq::QpskModulator modulator;

    std::vector<uint8_t> bits(20, 0);
    auto symbols = modulator.Modulate(bits);

    EXPECT_EQ(symbols.size(), 10);
}

TEST(QpskModulatorTest, UnitPower) {
    harq::QpskModulator modulator;

    for (uint8_t b0 : {0, 1}) {
        for (uint8_t b1 : {0, 1}) {
            auto symbols = modulator.Modulate({b0, b1});
            double power = std::norm(symbols[0]);
            EXPECT_NEAR(power, 1.0, EPSILON);
        }
    }
}

TEST(QpskDemodulatorTest, LlrSignConvention) {
    harq::QpskModulator modulator;
    harq::QpskDemodulator demodulator;

    auto symbols = modulator.Modulate({0, 0});
    double snr_linear = 10.0;

    auto demodulated = demodulator.Demodulate(symbols, snr_linear);

    EXPECT_LT(demodulated[0], 0);
    EXPECT_LT(demodulated[1], 0);
}

TEST(QpskDemodulatorTest, LlrMagnitude) {
    harq::QpskDemodulator demodulator;

    double snr_db = 10.0; 
    double snr_lin = std::pow(10.0, snr_db / 10.0); 
    std::complex<double> symbol(-1.0 / std::sqrt(2.0), -1.0 / std::sqrt(2.0));

    auto demodulated = demodulator.Demodulate({symbol}, snr_db);

    double expected = 4.0 * snr_lin * symbol.real(); 
    
    EXPECT_NEAR(std::abs(demodulated[0]), std::abs(expected), EPSILON);
}

TEST(QpskDemodulatorTest, DemodulateVector) {
    harq::QpskModulator modulator;
    harq::QpskDemodulator demodulator;

    std::vector<uint8_t> bits = {0, 1, 1, 0, 0, 0};
    auto symbols = modulator.Modulate(bits);

    auto llrs = demodulator.Demodulate(symbols, 20.0);

    EXPECT_EQ(llrs.size(), 6);

    EXPECT_LT(llrs[0], 0);
    EXPECT_GT(llrs[1], 0); 
}