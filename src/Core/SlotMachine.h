#pragma once

#include <array>
#include <memory>

#include "Config.h"
#include "Reel.h"
#include "Rng.h"
#include "Symbol.h"

namespace waffle {

inline constexpr int kReelCount = 3;

// One pull of the lever. The outcome is fully decided by the time this is
// returned — nothing about it depends on how the caller animates it (spec
// §2.1). nearMiss is true iff exactly two of the three symbols are WAFFLE
// and the spin isn't a jackpot; it exists so the render layer can play the
// "SO CLOSE" beat.
struct SpinResult {
    std::array<Symbol, kReelCount> symbols{};
    bool jackpot = false;
    bool nearMiss = false;
    // 1-based count of attempts since the last jackpot (or since start),
    // including this spin. Reset to 0 by a jackpot spin's *next* attempt.
    int attemptNumber = 0;
    // True if this spin's jackpot was forced by the pity timer rather than
    // landing naturally. Exposed for the debug overlay, not gameplay logic.
    bool wasPityTimer = false;
};

// Owns reel state, the RNG, and pity-timer/near-miss policy on top of it.
// Contains no Win32, no rendering, no I/O — usable identically from Demo
// and from the Credential Provider (spec §4.1).
class SlotMachine {
public:
    SlotMachine(Config config, std::unique_ptr<IRandomSource> randomSource);

    // Performs one pull. Never blocks, never throws.
    SpinResult Spin();

    // Replaces weights/pity-timer settings for subsequent spins without
    // resetting the attempt counter or RNG.
    void SetConfig(const Config& config);
    const Config& GetConfig() const { return config_; }

    int AttemptsSinceLastJackpot() const { return attemptsSinceLastJackpot_; }

    // Probability of a natural jackpot per spin given current weights,
    // ignoring the pity timer and near-miss bias. For the debug overlay
    // (spec §2, "Выведи в debug-оверлей фактическую вероятность").
    double NaturalJackpotProbability() const;

private:
    void RebuildReels();
    SpinResult DrawNatural();
    SpinResult ForcePityJackpot();
    void ApplyNearMissBoost(SpinResult& result);

    Config config_;
    Rng rng_;
    std::array<Reel, kReelCount> reels_;
    int attemptsSinceLastJackpot_ = 0;
};

}  // namespace waffle
