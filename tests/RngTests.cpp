#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "Core/Config.h"
#include "Core/Rng.h"

using waffle::Config;
using waffle::DeterministicRandomSource;
using waffle::Rng;
using waffle::Symbol;

// Spec §11: "генератор с детерминированным источником: воспроизводимые
// исходы".
TEST(RngTest, SameSeedProducesSameSequence) {
    const auto weights = Config::Default().reelWeights;

    Rng rngA(std::make_unique<DeterministicRandomSource>(12345));
    Rng rngB(std::make_unique<DeterministicRandomSource>(12345));

    std::vector<Symbol> sequenceA;
    std::vector<Symbol> sequenceB;
    for (int i = 0; i < 100; ++i) {
        sequenceA.push_back(rngA.PickWeighted(weights));
        sequenceB.push_back(rngB.PickWeighted(weights));
    }

    EXPECT_EQ(sequenceA, sequenceB);
}

TEST(RngTest, DifferentSeedsUsuallyDiverge) {
    const auto weights = Config::Default().reelWeights;

    Rng rngA(std::make_unique<DeterministicRandomSource>(1));
    Rng rngB(std::make_unique<DeterministicRandomSource>(2));

    std::vector<Symbol> sequenceA;
    std::vector<Symbol> sequenceB;
    for (int i = 0; i < 200; ++i) {
        sequenceA.push_back(rngA.PickWeighted(weights));
        sequenceB.push_back(rngB.PickWeighted(weights));
    }

    EXPECT_NE(sequenceA, sequenceB);
}

// Spec §11: "статистика: на 100 000 прогонов частота джекпота в пределах
// допуска от ожидаемой по весам" -- applied here to a single reel's
// per-symbol frequency, the building block the SlotMachine-level jackpot
// statistic (see SlotMachineTests.cpp) is built from.
TEST(RngTest, WeightedFrequencyMatchesExpectation) {
    Config config = Config::Default();
    Rng rng(std::make_unique<DeterministicRandomSource>(42));

    constexpr int kTrials = 100000;
    int waffleCount = 0;
    double totalWeight = 0.0;
    for (int w : config.reelWeights) {
        totalWeight += w;
    }
    const double expectedFraction =
        config.reelWeights[static_cast<std::size_t>(Symbol::Waffle)] / totalWeight;

    for (int i = 0; i < kTrials; ++i) {
        if (rng.PickWeighted(config.reelWeights) == Symbol::Waffle) {
            ++waffleCount;
        }
    }

    const double observedFraction = static_cast<double>(waffleCount) / kTrials;
    EXPECT_NEAR(observedFraction, expectedFraction, 0.01);
}

TEST(RngTest, AllZeroWeightsDegradesToWaffleRatherThanUB) {
    std::array<int, waffle::kSymbolCount> zeroWeights{};
    zeroWeights.fill(0);
    Rng rng(std::make_unique<DeterministicRandomSource>(1));

    EXPECT_EQ(rng.PickWeighted(zeroWeights), Symbol::Waffle);
}

TEST(RngTest, NextUniform01StaysInRange) {
    Rng rng(std::make_unique<DeterministicRandomSource>(7));
    for (int i = 0; i < 1000; ++i) {
        const double v = rng.NextUniform01();
        EXPECT_GE(v, 0.0);
        EXPECT_LT(v, 1.0);
    }
}
