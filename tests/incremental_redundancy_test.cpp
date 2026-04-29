#include "incremental_redundancy.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

TEST(IncrementalRedundancyTest, DefaultTransmissionSizeCoversUnevenCodewords) {
  EXPECT_EQ(harq::DefaultIrTransmissionSize(518, 4), 130);
  EXPECT_EQ(harq::DefaultIrTransmissionSize(512, 4), 128);
  EXPECT_EQ(harq::DefaultIrTransmissionSize(10, 4), 4);
}

TEST(IncrementalRedundancyTest, RateMatchUsesRedundancyVersionWindow) {
  const std::vector<uint8_t> codeword = {0, 0, 1, 1, 0, 1, 1, 0};
  const std::vector<std::size_t> identity = {0, 1, 2, 3, 4, 5, 6, 7};

  EXPECT_EQ(harq::RateMatch(codeword, 0, 2, identity, 4),
            (std::vector<uint8_t>{0, 0}));
  EXPECT_EQ(harq::RateMatch(codeword, 1, 2, identity, 4),
            (std::vector<uint8_t>{1, 1}));
  EXPECT_EQ(harq::RateMatch(codeword, 2, 2, identity, 4),
            (std::vector<uint8_t>{0, 1}));
  EXPECT_EQ(harq::RateMatch(codeword, 3, 2, identity, 4),
            (std::vector<uint8_t>{1, 0}));
}

TEST(IncrementalRedundancyTest, RedundancyVersionOffsetUsesTransmissionSize) {
  EXPECT_EQ(harq::RedundancyVersionOffset(780, 0, 512, 4), 0);
  EXPECT_EQ(harq::RedundancyVersionOffset(780, 1, 512, 4), 512);
  EXPECT_EQ(harq::RedundancyVersionOffset(780, 2, 512, 4), 244);
  EXPECT_EQ(harq::RedundancyVersionOffset(780, 3, 512, 4), 756);
}

TEST(IncrementalRedundancyTest, RateMatchRoundsTransmissionSizeToEven) {
  const std::vector<uint8_t> codeword = {0, 1, 0, 1, 1};
  const std::vector<std::size_t> identity = {0, 1, 2, 3, 4};

  EXPECT_EQ(harq::RateMatch(codeword, 0, 3, identity, 4).size(), 4);
  EXPECT_EQ(harq::RateMatch(codeword, 1, 3, identity, 4),
            (std::vector<uint8_t>{1, 0, 1, 0}));
}

TEST(IncrementalRedundancyTest, DematchCombinesAllPositionsForUnevenCodeword) {
  const std::vector<uint8_t> codeword = {0, 1, 0, 1, 1, 0, 1, 0, 0, 1};
  const std::vector<std::size_t> identity = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  std::vector<std::vector<double>> history;
  for (int rv = 0; rv < 4; ++rv) {
    const std::vector<uint8_t> tx = harq::RateMatch(codeword, rv, identity, 4);
    std::vector<double> soft;
    soft.reserve(tx.size());
    for (uint8_t bit : tx) {
      soft.push_back(bit ? 1.0 : -1.0);
    }
    history.push_back(soft);
  }

  const std::vector<double> combined =
      harq::CombineIncrementalRedundancySoft(history, codeword.size(), identity, 4);

  ASSERT_EQ(combined.size(), codeword.size());
  for (std::size_t i = 0; i < codeword.size(); ++i) {
    EXPECT_NE(combined[i], 0.0);
    EXPECT_EQ(combined[i] > 0.0, codeword[i] == 1);
  }
}

TEST(IncrementalRedundancyTest, RejectsPermutationWithWrongSize) {
  const std::vector<uint8_t> codeword = {0, 1, 0, 1};
  const std::vector<std::size_t> short_perm = {0, 1, 2};

  EXPECT_THROW(harq::RateMatch(codeword, 0, short_perm, 4),
               std::invalid_argument);
}
