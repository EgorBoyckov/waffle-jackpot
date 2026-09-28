#include "SlotMachine.h"

#include <algorithm>

namespace waffle {
namespace {

// Probability that a losing, non-2-waffle spin gets resampled into a
// deliberate near-miss (exactly two WAFFLE + one non-WAFFLE). Makes near
// misses "заметно чаще случайного" per spec §2.4 without touching
// IsJackpot or the odds of an actual jackpot. Internal tuning constant, not
// exposed in config.json — the schema in spec §7 doesn't define a knob for
// it.
constexpr double kNearMissBoostProbability = 0.35;

int CountWaffles(const std::array<Symbol, kReelCount>& symbols) {
    return static_cast<int>(std::count(symbols.begin(), symbols.end(), Symbol::Waffle));
}

}  // namespace

SlotMachine::SlotMachine(Config config, std::unique_ptr<IRandomSource> randomSource)
    : config_(config), rng_(std::move(randomSource)), reels_{Reel(config.reelWeights),
                                                               Reel(config.reelWeights),
                                                               Reel(config.reelWeights)} {}

void SlotMachine::SetConfig(const Config& config) {
    config_ = config;
    RebuildReels();
}

void SlotMachine::RebuildReels() {
    for (auto& reel : reels_) {
        reel.SetWeights(config_.reelWeights);
    }
}

SpinResult SlotMachine::DrawNatural() {
    SpinResult result;
    for (int i = 0; i < kReelCount; ++i) {
        result.symbols[static_cast<std::size_t>(i)] = reels_[static_cast<std::size_t>(i)].Draw(rng_);
    }
    result.jackpot = IsJackpot(result.symbols[0], result.symbols[1], result.symbols[2]);
    result.nearMiss = !result.jackpot && CountWaffles(result.symbols) == 2;
    return result;
}

SpinResult SlotMachine::ForcePityJackpot() {
    SpinResult result;
    result.symbols.fill(Symbol::Waffle);
    result.jackpot = true;
    result.nearMiss = false;
    result.wasPityTimer = true;
    return result;
}

void SlotMachine::ApplyNearMissBoost(SpinResult& result) {
    if (result.jackpot || result.nearMiss) {
        return;
    }
    if (rng_.NextUniform01() >= kNearMissBoostProbability) {
        return;
    }

    // Force exactly two WAFFLE symbols; draw the third from a version of
    // this reel's weights with WAFFLE excluded so it can't accidentally
    // become a jackpot, and re-draw once if it happens to still be WAFFLE
    // (e.g. WAFFLE was the only weighted symbol).
    const std::size_t offPosition = static_cast<std::size_t>(
        rng_.NextUniform01() * kReelCount) % kReelCount;

    std::array<int, kSymbolCount> nonWaffleWeights = config_.reelWeights;
    nonWaffleWeights[static_cast<std::size_t>(Symbol::Waffle)] = 0;
    const bool hasNonWaffleWeight =
        std::any_of(nonWaffleWeights.begin(), nonWaffleWeights.end(), [](int w) { return w > 0; });
    if (!hasNonWaffleWeight) {
        // Every symbol but WAFFLE has zero weight; boosting would just
        // produce another jackpot, so leave the natural result alone.
        return;
    }

    for (std::size_t i = 0; i < kReelCount; ++i) {
        result.symbols[i] = (i == offPosition) ? reels_[i].Draw(rng_) : Symbol::Waffle;
    }
    while (result.symbols[offPosition] == Symbol::Waffle) {
        Reel nonWaffleReel(nonWaffleWeights);
        result.symbols[offPosition] = nonWaffleReel.Draw(rng_);
    }

    result.jackpot = false;
    result.nearMiss = true;
}

SpinResult SlotMachine::Spin() {
    ++attemptsSinceLastJackpot_;

    SpinResult result;
    const bool pityDue = config_.guaranteedJackpotAfterAttempts > 0 &&
                          attemptsSinceLastJackpot_ >= config_.guaranteedJackpotAfterAttempts;

    if (pityDue) {
        result = ForcePityJackpot();
    } else {
        result = DrawNatural();
        if (!result.jackpot) {
            ApplyNearMissBoost(result);
        }
    }

    result.attemptNumber = attemptsSinceLastJackpot_;

    if (result.jackpot) {
        attemptsSinceLastJackpot_ = 0;
    }

    return result;
}

double SlotMachine::NaturalJackpotProbability() const {
    double total = 0.0;
    for (int w : config_.reelWeights) {
        total += w;
    }
    if (total <= 0.0) {
        return 0.0;
    }
    const double waffleWeight = static_cast<double>(config_.reelWeights[static_cast<std::size_t>(Symbol::Waffle)]);
    const double perReelProbability = waffleWeight / total;
    return perReelProbability * perReelProbability * perReelProbability;
}

}  // namespace waffle
