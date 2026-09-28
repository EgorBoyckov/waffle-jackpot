#include "Config.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace waffle {
namespace {

using nlohmann::json;

constexpr int kMinGuaranteedJackpotAttempts = 1;
constexpr int kMaxGuaranteedJackpotAttempts = 50;
constexpr int kMinDurationMs = 0;
constexpr int kMaxDurationMs = 60000;
constexpr int kMinReelWeight = 0;
constexpr int kMaxReelWeight = 1000;

void Warn(std::vector<std::string>* warnings, const std::string& message) {
    if (warnings) {
        warnings->push_back(message);
    }
}

const char* SymbolKey(Symbol s) {
    // Matches the JSON key spelling in project spec §7's reelWeights map
    // and jackpotSymbol field (uppercase names, same as ToString()).
    return ToString(s);
}

bool TryParseSymbolKey(const std::string& key, Symbol* outSymbol) {
    for (std::size_t i = 0; i < kSymbolCount; ++i) {
        const auto symbol = static_cast<Symbol>(i);
        if (key == SymbolKey(symbol)) {
            *outSymbol = symbol;
            return true;
        }
    }
    return false;
}

template <typename T>
T ClampWithWarning(std::vector<std::string>* warnings, const char* field, T value, T lo, T hi) {
    if (value < lo || value > hi) {
        std::ostringstream oss;
        oss << "config: '" << field << "' out of range (" << value << "), clamped to ["
            << lo << ", " << hi << "]";
        Warn(warnings, oss.str());
        return std::clamp(value, lo, hi);
    }
    return value;
}

void ApplyField(const json& j, const char* key, bool& target) {
    if (j.contains(key) && j[key].is_boolean()) {
        target = j[key].get<bool>();
    }
}

}  // namespace

Config Config::Default() {
    return Config{};
}

Config ConfigLoader::LoadFromString(const std::string& jsonText,
                                     std::vector<std::string>* outWarnings) {
    Config config = Config::Default();

    if (jsonText.empty()) {
        Warn(outWarnings, "config: empty, using defaults");
        return config;
    }

    json j;
    try {
        j = json::parse(jsonText);
    } catch (const json::parse_error& e) {
        Warn(outWarnings, std::string("config: malformed JSON (") + e.what() + "), using defaults");
        return Config::Default();
    }

    if (!j.is_object()) {
        Warn(outWarnings, "config: root is not a JSON object, using defaults");
        return Config::Default();
    }

    // schemaVersion: only version 1 is understood. Anything else falls back
    // to full defaults rather than guessing at a future/older schema.
    if (j.contains("schemaVersion")) {
        if (j["schemaVersion"].is_number_integer() && j["schemaVersion"].get<int>() == 1) {
            config.schemaVersion = 1;
        } else {
            Warn(outWarnings, "config: unsupported schemaVersion, using defaults");
            return Config::Default();
        }
    }

    ApplyField(j, "enabled", config.enabled);

    // jackpotSymbol / reels are fixed in v1 (spec §7). A file that asks for
    // anything else gets a warning and the fixed default, not what it
    // asked for.
    if (j.contains("jackpotSymbol") && j["jackpotSymbol"].is_string()) {
        Symbol parsed;
        if (TryParseSymbolKey(j["jackpotSymbol"].get<std::string>(), &parsed) &&
            parsed == Symbol::Waffle) {
            config.jackpotSymbol = Symbol::Waffle;
        } else {
            Warn(outWarnings, "config: 'jackpotSymbol' must be WAFFLE in schema v1, ignoring");
        }
    }
    if (j.contains("reels") && j["reels"].is_number_integer() && j["reels"].get<int>() != 3) {
        Warn(outWarnings, "config: 'reels' must be 3 in schema v1, ignoring");
    }
    config.jackpotSymbol = Symbol::Waffle;
    config.reels = 3;

    if (j.contains("reelWeights") && j["reelWeights"].is_object()) {
        std::array<int, kSymbolCount> weights = config.reelWeights;
        for (auto it = j["reelWeights"].begin(); it != j["reelWeights"].end(); ++it) {
            Symbol symbol;
            if (!TryParseSymbolKey(it.key(), &symbol)) {
                Warn(outWarnings, "config: unknown reelWeights key '" + it.key() + "', ignoring");
                continue;
            }
            if (!it.value().is_number_integer()) {
                Warn(outWarnings, "config: reelWeights['" + it.key() + "'] is not an integer, ignoring");
                continue;
            }
            const int raw = it.value().get<int>();
            weights[static_cast<std::size_t>(symbol)] =
                ClampWithWarning(outWarnings, it.key().c_str(), raw, kMinReelWeight, kMaxReelWeight);
        }
        const bool anyPositive = std::any_of(weights.begin(), weights.end(), [](int w) { return w > 0; });
        if (anyPositive) {
            config.reelWeights = weights;
        } else {
            Warn(outWarnings, "config: reelWeights are all zero, using defaults");
        }
    }

    if (j.contains("guaranteedJackpotAfterAttempts") && j["guaranteedJackpotAfterAttempts"].is_number_integer()) {
        config.guaranteedJackpotAfterAttempts = ClampWithWarning(
            outWarnings, "guaranteedJackpotAfterAttempts",
            j["guaranteedJackpotAfterAttempts"].get<int>(), kMinGuaranteedJackpotAttempts,
            kMaxGuaranteedJackpotAttempts);
    }

    ApplyField(j, "resetJackpotOnFailedLogon", config.resetJackpotOnFailedLogon);

    if (j.contains("animationDurationMs") && j["animationDurationMs"].is_number_integer()) {
        config.animationDurationMs = ClampWithWarning(outWarnings, "animationDurationMs",
                                                        j["animationDurationMs"].get<int>(),
                                                        kMinDurationMs, kMaxDurationMs);
    }
    if (j.contains("jackpotSequenceMs") && j["jackpotSequenceMs"].is_number_integer()) {
        config.jackpotSequenceMs = ClampWithWarning(outWarnings, "jackpotSequenceMs",
                                                      j["jackpotSequenceMs"].get<int>(),
                                                      kMinDurationMs, kMaxDurationMs);
    }
    ApplyField(j, "respectReducedMotion", config.respectReducedMotion);

    ApplyField(j, "soundEnabled", config.soundEnabled);
    ApplyField(j, "logonSoundEnabled", config.logonSoundEnabled);
    if (j.contains("volume") && j["volume"].is_number()) {
        config.volume = ClampWithWarning(outWarnings, "volume", j["volume"].get<double>(), 0.0, 1.0);
    }

    ApplyField(j, "demoMode", config.demoMode);
    ApplyField(j, "debugMode", config.debugMode);

    ApplyField(j, "showInRemoteSessions", config.showInRemoteSessions);
    ApplyField(j, "experimentalFilterMode", config.experimentalFilterMode);

    ApplyField(j, "allowEmergencyBypass", config.allowEmergencyBypass);
    if (!config.allowEmergencyBypass) {
        // Spec §7: never let the only way into the machine be switched off.
        Warn(outWarnings, "config: 'allowEmergencyBypass=false' is not honored, forcing true");
        config.allowEmergencyBypass = true;
    }

    return config;
}

Config ConfigLoader::LoadFromFile(const std::filesystem::path& path,
                                   std::vector<std::string>* outWarnings) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        Warn(outWarnings, "config: could not open '" + path.string() + "', using defaults");
        return Config::Default();
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return LoadFromString(buffer.str(), outWarnings);
}

std::string ConfigLoader::ToJsonString(const Config& config) {
    json j;
    j["schemaVersion"] = config.schemaVersion;
    j["enabled"] = config.enabled;
    j["jackpotSymbol"] = SymbolKey(config.jackpotSymbol);
    j["reels"] = config.reels;

    json weights = json::object();
    for (std::size_t i = 0; i < kSymbolCount; ++i) {
        const auto symbol = static_cast<Symbol>(i);
        weights[SymbolKey(symbol)] = config.reelWeights[i];
    }
    j["reelWeights"] = weights;

    j["guaranteedJackpotAfterAttempts"] = config.guaranteedJackpotAfterAttempts;
    j["resetJackpotOnFailedLogon"] = config.resetJackpotOnFailedLogon;

    j["animationDurationMs"] = config.animationDurationMs;
    j["jackpotSequenceMs"] = config.jackpotSequenceMs;
    j["respectReducedMotion"] = config.respectReducedMotion;

    j["soundEnabled"] = config.soundEnabled;
    j["logonSoundEnabled"] = config.logonSoundEnabled;
    j["volume"] = config.volume;

    j["demoMode"] = config.demoMode;
    j["debugMode"] = config.debugMode;

    j["showInRemoteSessions"] = config.showInRemoteSessions;
    j["experimentalFilterMode"] = config.experimentalFilterMode;
    j["allowEmergencyBypass"] = config.allowEmergencyBypass;

    return j.dump(2);
}

}  // namespace waffle
