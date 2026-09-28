#include <gtest/gtest.h>

#include <cmath>

#include "Core/Config.h"
#include "Core/Rng.h"
#include "Core/SlotMachine.h"

using waffle::Config;
using waffle::DeterministicRandomSource;
using waffle::SlotMachine;
using waffle::Symbol;

namespace {
Config ConfigWithoutNaturalWaffles() {
    // Zeroes WAFFLE weight so only the pity timer can produce a jackpot --
    // isolates the guarantee from natural odds.
    Config config = Config::Default();
    config.reelWeights[static_cast<std::size_t>(Symbol::Waffle)] = 0;
    config.reelWeights[static_cast<std::size_t>(Symbol::Cherry)] = 1;
    return config;
}
}  // namespace

// Spec §11: "pity timer: джекпот гарантирован ровно на N-й попытке,
// счётчик сбрасывается после джекпота".
TEST(SlotMachineTest, PityTimerFiresExactlyOnNthAttempt) {
    Config config = ConfigWithoutNaturalWaffles();
    config.guaranteedJackpotAfterAttempts = 8;
    SlotMachine machine(config, std::make_unique<DeterministicRandomSource>(1));

    for (int attempt = 1; attempt <= 7; ++attempt) {
        const auto result = machine.Spin();
        EXPECT_FALSE(result.jackpot) << "unexpected jackpot at attempt " << attempt;
        EXPECT_EQ(result.attemptNumber, attempt);
    }

    const auto result = machine.Spin();
    EXPECT_TRUE(result.jackpot);
    EXPECT_TRUE(result.wasPityTimer);
    EXPECT_EQ(result.attemptNumber, 8);
    EXPECT_EQ(result.symbols[0], Symbol::Waffle);
    EXPECT_EQ(result.symbols[1], Symbol::Waffle);
    EXPECT_EQ(result.symbols[2], Symbol::Waffle);
}

TEST(SlotMachineTest, AttemptCounterResetsAfterJackpot) {
    Config config = ConfigWithoutNaturalWaffles();
    config.guaranteedJackpotAfterAttempts = 5;
    SlotMachine machine(config, std::make_unique<DeterministicRandomSource>(2));

    for (int i = 0; i < 4; ++i) {
        machine.Spin();
    }
    const auto jackpotResult = machine.Spin();
    ASSERT_TRUE(jackpotResult.jackpot);
    EXPECT_EQ(machine.AttemptsSinceLastJackpot(), 0);

    const auto nextResult = machine.Spin();
    EXPECT_EQ(nextResult.attemptNumber, 1);
}

TEST(SlotMachineTest, NaturalJackpotBeforePityTimerAlsoResetsCounter) {
    // All-WAFFLE weights: every spin is a natural jackpot, well before any
    // plausible pity timer setting.
    Config config = Config::Default();
    for (auto& w : config.reelWeights) {
        w = 0;
    }
    config.reelWeights[static_cast<std::size_t>(Symbol::Waffle)] = 1;
    config.guaranteedJackpotAfterAttempts = 50;
    SlotMachine machine(config, std::make_unique<DeterministicRandomSource>(3));

    const auto result = machine.Spin();
    EXPECT_TRUE(result.jackpot);
    EXPECT_FALSE(result.wasPityTimer);
    EXPECT_EQ(machine.AttemptsSinceLastJackpot(), 0);
}

// Spec §11 / §2.4: near misses should land noticeably more often than
// their natural (un-boosted) weighted probability.
TEST(SlotMachineTest, NearMissIsBoostedAboveNaturalFrequency) {
    Config config = Config::Default();
    config.guaranteedJackpotAfterAttempts = 1000000;  // effectively disabled for this measurement
    SlotMachine machine(config, std::make_unique<DeterministicRandomSource>(1234));

    constexpr int kTrials = 100000;
    int nearMissCount = 0;
    for (int i = 0; i < kTrials; ++i) {
        if (machine.Spin().nearMiss) {
            ++nearMissCount;
        }
    }

    double totalWeight = 0.0;
    for (int w : config.reelWeights) {
        totalWeight += w;
    }
    const double p = config.reelWeights[static_cast<std::size_t>(Symbol::Waffle)] / totalWeight;
    const double naturalTwoWaffles = 3.0 * p * p * (1.0 - p);

    const double observed = static_cast<double>(nearMissCount) / kTrials;
    EXPECT_GT(observed, naturalTwoWaffles * 1.1);
}

TEST(SlotMachineTest, NearMissNeverCoincidesWithJackpot) {
    Config config = Config::Default();
    SlotMachine machine(config, std::make_unique<DeterministicRandomSource>(555));

    for (int i = 0; i < 20000; ++i) {
        const auto result = machine.Spin();
        EXPECT_FALSE(result.jackpot && result.nearMiss);
    }
}

TEST(SlotMachineTest, NaturalJackpotProbabilityMatchesFormula) {
    Config config = Config::Default();
    SlotMachine machine(config, std::make_unique<DeterministicRandomSource>(9));

    double totalWeight = 0.0;
    for (int w : config.reelWeights) {
        totalWeight += w;
    }
    const double p = config.reelWeights[static_cast<std::size_t>(Symbol::Waffle)] / totalWeight;
    const double expected = p * p * p;

    EXPECT_NEAR(machine.NaturalJackpotProbability(), expected, 1e-9);
}

// Note: ApplyNearMissBoost can deliberately overwrite reel positions with
// WAFFLE regardless of configured weights -- a near miss is "two WAFFLEs,
// third symbol different" by definition (spec §2.4), independent of
// reelWeights. So a single Lemon-only spin isn't guaranteed to show only
// Lemon; this checks the statistical effect of SetConfig instead.
TEST(SlotMachineTest, SetConfigChangesSubsequentSpins) {
    Config config = Config::Default();
    for (auto& w : config.reelWeights) {
        w = 0;
    }
    config.reelWeights[static_cast<std::size_t>(Symbol::Lemon)] = 1;
    config.guaranteedJackpotAfterAttempts = 1000000;  // effectively disabled
    SlotMachine machine(config, std::make_unique<DeterministicRandomSource>(4));

    bool sawLemonBefore = false;
    for (int i = 0; i < 50; ++i) {
        for (Symbol s : machine.Spin().symbols) {
            sawLemonBefore |= (s == Symbol::Lemon);
        }
    }
    EXPECT_TRUE(sawLemonBefore);

    Config updated = config;
    for (auto& w : updated.reelWeights) {
        w = 0;
    }
    updated.reelWeights[static_cast<std::size_t>(Symbol::Bell)] = 1;
    machine.SetConfig(updated);

    bool sawLemonAfter = false;
    bool sawBellAfter = false;
    for (int i = 0; i < 50; ++i) {
        for (Symbol s : machine.Spin().symbols) {
            sawLemonAfter |= (s == Symbol::Lemon);
            sawBellAfter |= (s == Symbol::Bell);
        }
    }
    EXPECT_FALSE(sawLemonAfter);
    EXPECT_TRUE(sawBellAfter);
}

TEST(SlotMachineTest, PityTimerDisabledWhenZero) {
    Config config = ConfigWithoutNaturalWaffles();
    config.guaranteedJackpotAfterAttempts = 0;
    SlotMachine machine(config, std::make_unique<DeterministicRandomSource>(11));

    for (int i = 0; i < 1000; ++i) {
        EXPECT_FALSE(machine.Spin().jackpot);
    }
}
