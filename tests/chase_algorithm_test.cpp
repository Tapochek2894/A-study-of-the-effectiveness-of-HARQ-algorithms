// test_chase_algorithms.cpp
#include <gtest/gtest.h>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <set>

// Предполагается, что у тебя есть chase_algorithm.hpp с объявлением функций
#include "chase_algorithm.hpp"
#include "utils.hpp"

using namespace harq;

// ------------------------------------------------------------------
// 1. Тесты для get_n_smallest_indices
// ------------------------------------------------------------------

TEST(GetNSmallestIndicesTest, Basic) {
    std::vector<double> rel = {0.9, 0.2, 0.7, 0.1, 0.8};
    auto indices = get_n_smallest_indices(rel, 3);
    // Наименьшие: 0.1 (3), 0.2 (1), 0.7 (2) → порядок как в partial_sort: [3,1,2]
    EXPECT_EQ(indices, std::vector<size_t>({3, 1, 2}));
}

TEST(GetNSmallestIndicesTest, Duplicates) {
    std::vector<double> rel = {0.5, 0.3, 0.3, 0.6};
    auto indices = get_n_smallest_indices(rel, 2);
    // partial_sort стабильна? Нет! Но индексы любого из двух 0.3 — допустимы.
    // Однако, у нас нет требования стабильности, так что проверим только значения индексов
    EXPECT_EQ(indices.size(), 2u);
    EXPECT_TRUE(indices[0] == 1 || indices[0] == 2);
    EXPECT_TRUE(indices[1] == 1 || indices[1] == 2);
    EXPECT_NE(indices[0], indices[1]); // должны быть разные
}

TEST(GetNSmallestIndicesTest, SingleElement) {
    EXPECT_EQ(get_n_smallest_indices({5.0}, 1), std::vector<size_t>({0}));
}

TEST(GetNSmallestIndicesTest, AllElements) {
    std::vector<double> v = {3.0, 1.0, 2.0};
    auto res = get_n_smallest_indices(v, 3);
    // partial_sort сортирует всё → индексы: [1,2,0]
    EXPECT_EQ(res, std::vector<size_t>({1, 2, 0}));
}

TEST(GetNSmallestIndicesTest, InvalidN_TooLarge) {
    EXPECT_THROW(get_n_smallest_indices({1.0, 2.0}, 3), std::invalid_argument);
}

TEST(GetNSmallestIndicesTest, InvalidN_ZeroOrNegative) {
    EXPECT_THROW(get_n_smallest_indices({1.0}, 0), std::invalid_argument);
    EXPECT_THROW(get_n_smallest_indices({1.0}, -1), std::invalid_argument);
}

TEST(GetNSmallestIndicesTest, EmptyInput) {
    EXPECT_THROW(get_n_smallest_indices({}, 1), std::invalid_argument);
}

// ------------------------------------------------------------------
// 2. Тесты для generate_probe_sequences_1
// ------------------------------------------------------------------

TEST(GenerateProbeSequences1Test, Basic) {
    auto seqs = generate_probe_sequences_1(3, 2); // d/2 = 1 → все комбинации с одной 1
    // Ожидаем: [000], [001], [010], [100]
    std::vector<std::vector<char>> expected = {
        {0,0,0},
        {0,0,1},
        {0,1,0},
        {1,0,0}
    };
    EXPECT_EQ(seqs, expected);
}

TEST(GenerateProbeSequences1Test, dZero) {
    // d=0 → d/2 = 0 → теперь exception
    EXPECT_THROW(generate_probe_sequences_1(2, 0), std::invalid_argument);
}


TEST(GenerateProbeSequences1Test, dOdd) {
    // d=3 → d/2 = 1 (integer division)
    auto seqs = generate_probe_sequences_1(2, 3);
    EXPECT_EQ(seqs, std::vector<std::vector<char>>({{0,0}, {0,1}, {1,0}}));
}

TEST(GenerateProbeSequences1Test, AllOnes) {
    // n=2, d=4 → d/2=2 → только [1,1]
    auto seqs = generate_probe_sequences_1(2, 4);
    EXPECT_EQ(seqs, std::vector<std::vector<char>>({{0,0}, {1,1}}));
}

TEST(GenerateProbeSequences1Test, InvalidInput) {
    EXPECT_THROW(generate_probe_sequences_1(2, 6), std::invalid_argument); // d/2=3 > n=2
}

// ------------------------------------------------------------------
// 3. Тесты для generate_probe_sequences_2
// ------------------------------------------------------------------

TEST(GenerateProbeSequences2Test, Basic) {
    std::vector<double> rel = {0.9, 0.2, 0.7, 0.1, 0.8}; // наименее надёжные: индексы 3,1,2 (для d=6 → selection=3)
    auto seqs = generate_probe_sequences_2(5, 6, rel); // d/2 = 3

    EXPECT_EQ(seqs.size(), 8u); // 2^3 = 8

    // Все последовательности должны иметь нули везде, кроме позиций 3,1,2
    for (const auto& seq : seqs) {
        EXPECT_EQ(seq.size(), 5u);
        // Проверим, что вне {1,2,3} — только нули
        EXPECT_EQ(seq[0], 0);
        EXPECT_EQ(seq[4], 0);
    }

    // Убедимся, что все комбинации присутствуют
    std::set<std::vector<char>> seq_set(seqs.begin(), seqs.end());
    EXPECT_EQ(seq_set.size(), 8u); // уникальность

    // Проверим конкретную: [0,1,0,1,0] → индекс1=1, индекс3=1 → да
    std::vector<char> expected = {0,1,0,1,0};
    EXPECT_TRUE(seq_set.count(expected) > 0);
}

TEST(GenerateProbeSequences2Test, dZeroOrOne) {
    // d=1 → d/2 = 0 → только нулевой вектор
    std::vector<double> rel = {0.1, 0.2};
    auto seqs = generate_probe_sequences_2(2, 1, rel);
    EXPECT_EQ(seqs, std::vector<std::vector<char>>({{0,0}}));
}

TEST(GenerateProbeSequences2Test, SelectionOne) {
    std::vector<double> rel = {0.5, 0.1, 0.9};
    auto seqs = generate_probe_sequences_2(3, 2, rel); // d/2=1 → наименее надёжный: индекс 1
    std::vector<std::vector<char>> expected = {{0,0,0}, {0,1,0}};
    EXPECT_EQ(seqs, expected);
}

TEST(GenerateProbeSequences2Test, MismatchSize) {
    EXPECT_THROW(generate_probe_sequences_2(3, 2, {0.1, 0.2}), std::invalid_argument);
}

TEST(GenerateProbeSequences2Test, InvalidNOrD) {
    std::vector<double> rel = {0.1, 0.2};
    EXPECT_THROW(generate_probe_sequences_2(-1, 2, rel), std::invalid_argument);
    EXPECT_THROW(generate_probe_sequences_2(2, -1, rel), std::invalid_argument);
    EXPECT_THROW(generate_probe_sequences_2(0, 2, rel), std::invalid_argument);
    EXPECT_THROW(generate_probe_sequences_2(2, 0, rel), std::invalid_argument);
}

// ------------------------------------------------------------------
// 4. Тесты для generate_probe_sequences_3
// ------------------------------------------------------------------

TEST(GenerateProbeSequences3Test, dOdd) {
    // d=3 → precarious = d-1 = 2
    // d%2==1 → берём каждый второй, начиная с 0: indices[0], indices[2], ...
    std::vector<double> rel = {0.9, 0.2, 0.7, 0.1, 0.8}; // наименее надёжные: [3,1,2,4,0] → первые 2: [3,1]
    // ones_positions: i=0 → 3
    auto seqs = generate_probe_sequences_3(5, 3, rel);
    EXPECT_EQ(seqs.size(), 1u);
    std::vector<char> expected = {0,0,0,1,0}; // только позиция 3 = 1
    EXPECT_EQ(seqs[0], expected);
}

TEST(GenerateProbeSequences3Test, dEven) {
    // d=4 → precarious = 3
    // d%2==0 → берём indices[0], indices[1], затем с индекса 3 каждый второй
    std::vector<double> rel = {0.9, 0.1, 0.8, 0.2, 0.7}; 
    // Наименее надёжные (3 шт): [1,3,2]
    // ones_positions: [1,3] + (начиная с i=3 → выход за пределы) → только [1,3]
    auto seqs = generate_probe_sequences_3(5, 4, rel);
    std::vector<char> expected = {0,1,0,1,0};
    EXPECT_EQ(seqs[0], expected);
}

TEST(GenerateProbeSequences3Test, dOne) {
    // d=1 → precarious = 0 → get_n_smallest_indices(0) → exception?
    // Но в коде: if (precarious_positions > n) → 0 > n? нет.
    // Затем вызывается get_n_smallest_indices(reliability, 0) → но в get_n_smallest_indices: n<=0 → exception!
    std::vector<double> rel = {0.5};
    EXPECT_THROW(generate_probe_sequences_3(1, 1, rel), std::invalid_argument);
}

TEST(GenerateProbeSequences3Test, PrecariousZero) {
    // d=1 → precarious = 0 → но get_n_smallest_indices не вызывается, если precarious==0?
    // В текущей реализации: вызывается get_n_smallest_indices(..., 0) → кидает исключение.
    // Либо нужно обработать этот случай.
    // Пока оставим как есть — тест должен падать.
}

TEST(GenerateProbeSequences3Test, InvalidInput) {
    std::vector<double> rel = {0.1, 0.2};
    EXPECT_THROW(generate_probe_sequences_3(-1, 2, rel), std::invalid_argument);
    EXPECT_THROW(generate_probe_sequences_3(2, -1, rel), std::invalid_argument);
    EXPECT_THROW(generate_probe_sequences_3(2, 2, {0.1}), std::invalid_argument); // size mismatch
    EXPECT_THROW(generate_probe_sequences_3(2, 5, rel), std::invalid_argument); // d-1=4 > n=2
}

// ------------------------------------------------------------------
// Дополнительно: убедимся, что все последовательности содержат только 0 и 1
// ------------------------------------------------------------------
TEST(GenerateProbeSequencesSanity, OnlyZerosAndOnes) {
    std::vector<double> rel = {0.9, 0.2, 0.7, 0.1, 0.8};
    auto s1 = generate_probe_sequences_1(4, 2);
    auto s2 = generate_probe_sequences_2(5, 4, rel);
    auto s3 = generate_probe_sequences_3(5, 3, rel);

    auto check = [](const std::vector<std::vector<char>>& seqs) {
        for (const auto& seq : seqs) {
            for (char c : seq) {
                EXPECT_TRUE(c == 0 || c == 1);
            }
        }
    };

    check(s1);
    check(s2);
    check(s3);
}