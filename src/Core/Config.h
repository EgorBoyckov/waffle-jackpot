#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "Symbol.h"

namespace waffle {

// Mirrors config/config.default.json (project spec §7). This struct is the
// only representation of config used anywhere in the codebase — Core,
// Render, Demo and the Credential Provider all read this, never raw JSON.
struct Config {
    int schemaVersion = 1;
    bool enabled = true;

    // Fixed in v1: always WAFFLE / 3 reels. Present as fields (rather than
    // constants) so a future schema version can vary them without an ABI
    // break, but LoadFromString rejects any other value today (spec §7:
    // "параметры существуют для будущего, а не для того, чтобы кто-то
    // сделал «джекпот по семёркам»").
    Symbol jackpotSymbol = Symbol::Waffle;
    int reels = 3;

    // Indexed by static_cast<size_t>(Symbol). Defaults match spec §7.
    std::array<int, kSymbolCount> reelWeights = {
        5,  // Waffle
        2,  // Cherry
        2,  // Lemon
        2,  // Bell
        1,  // Diamond
        1,  // Star
        1,  // Seven
        1,  // Lucky
    };

    int guaranteedJackpotAfterAttempts = 8;
    bool resetJackpotOnFailedLogon = true;

    int animationDurationMs = 2500;
    int jackpotSequenceMs = 3000;
    bool respectReducedMotion = true;

    bool soundEnabled = true;
    bool logonSoundEnabled = false;
    double volume = 0.6;

    bool demoMode = false;
    bool debugMode = false;

    bool showInRemoteSessions = false;
    bool experimentalFilterMode = false;
    bool allowEmergencyBypass = true;

    static Config Default();
};

// Loads and validates config per project spec §7:
//   - malformed/missing/empty JSON -> Default(), never throws;
//   - every numeric field is clamped into a sane range;
//   - jackpotSymbol/reels forced back to WAFFLE/3 if the file says
//     otherwise;
//   - allowEmergencyBypass=false is always overridden back to true, because
//     the config is the only way in and a self-lockout is unacceptable in
//     the logon path (spec §7: "Лучше вообще не давать выключать bypass").
// Any deviation the loader corrects is appended to outWarnings (may be
// null if the caller doesn't care) as a human-readable string, for the
// debug overlay / Control.exe status output. Never throws.
class ConfigLoader {
public:
    static Config LoadFromString(const std::string& jsonText,
                                  std::vector<std::string>* outWarnings = nullptr);

    // Missing file -> Default() with a warning, same as malformed JSON.
    static Config LoadFromFile(const std::filesystem::path& path,
                                std::vector<std::string>* outWarnings = nullptr);

    // Serializes back to JSON text (pretty-printed), e.g. for
    // WaffleJackpotControl.exe to write out a fresh config.
    static std::string ToJsonString(const Config& config);
};

}  // namespace waffle
