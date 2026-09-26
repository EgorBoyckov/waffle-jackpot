#include <gtest/gtest.h>

#include <array>

#include "Core/Reel.h"
#include "Core/Rng.h"

using waffle::DeterministicRandomSource;
using waffle::Reel;
using waffle::Rng;
using waffle::Symbol;
using waffle::kSymbolCount;

namespace {
std::array<int, kSymbolCount> ZeroWeights() {
    std::array<int, kSymbolCount> weights{};
    weights.fill(0);
    return weights;
}
}  // namespace

TEST(ReelTest, SingleWeightedSymbolAlwaysDraws) {
    auto weights = ZeroWeights();
    weights[static_cast<std::size_t>(Symbol::Lemon)] = 1;
    Reel reel(weights);
    Rng rng(std::make_unique<DeterministicRandomSource>(1));

    for (int i = 0; i < 200; ++i) {
        EXPECT_EQ(reel.Draw(rng), Symbol::Lemon);
    }
}

TEST(ReelTest, NeverDrawsAZeroWeightSymbol) {
    auto weights = ZeroWeights();
    weights[static_cast<std::size_t>(Symbol::Waffle)] = 3;
    weights[static_cast<std::size_t>(Symbol::Bell)] = 1;
    Reel reel(weights);
    Rng rng(std::make_unique<DeterministicRandomSource>(99));

    for (int i = 0; i < 500; ++i) {
        const Symbol drawn = reel.Draw(rng);
        EXPECT_TRUE(drawn == Symbol::Waffle || drawn == Symbol::Bell) << "drew " << waffle::ToString(drawn);
    }
}

TEST(ReelTest, SetWeightsChangesSubsequentDraws) {
    auto weights = ZeroWeights();
    weights[static_cast<std::size_t>(Symbol::Cherry)] = 1;
    Reel reel(weights);
    Rng rng(std::make_unique<DeterministicRandomSource>(7));
    EXPECT_EQ(reel.Draw(rng), Symbol::Cherry);

    auto newWeights = ZeroWeights();
    newWeights[static_cast<std::size_t>(Symbol::Diamond)] = 1;
    reel.SetWeights(newWeights);
    EXPECT_EQ(reel.Draw(rng), Symbol::Diamond);
}

TEST(ReelTest, RoughlyMatchesWeightDistribution) {
    auto weights = ZeroWeights();
    weights[static_cast<std::size_t>(Symbol::Waffle)] = 1;
    weights[static_cast<std::size_t>(Symbol::Cherry)] = 3;
    Reel reel(weights);
    Rng rng(std::make_unique<DeterministicRandomSource>(2024));

    constexpr int kTrials = 20000;
    int waffleCount = 0;
    for (int i = 0; i < kTrials; ++i) {
        if (reel.Draw(rng) == Symbol::Waffle) {
            ++waffleCount;
        }
    }

    // Expected fraction is 1/4; allow generous statistical slack.
    const double fraction = static_cast<double>(waffleCount) / kTrials;
    EXPECT_NEAR(fraction, 0.25, 0.02);
}
