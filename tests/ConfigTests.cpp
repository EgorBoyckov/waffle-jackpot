#include <gtest/gtest.h>

#include "Core/Config.h"

using waffle::Config;
using waffle::ConfigLoader;
using waffle::Symbol;

TEST(ConfigTest, ValidConfigParsesWithNoWarnings) {
    constexpr char kJson[] = R"({
        "schemaVersion": 1,
        "enabled": true,
        "jackpotSymbol": "WAFFLE",
        "reels": 3,
        "reelWeights": {"WAFFLE": 5, "CHERRY": 2, "LEMON": 2, "BELL": 2,
                         "DIAMOND": 1, "STAR": 1, "SEVEN": 1, "LUCKY": 1},
        "guaranteedJackpotAfterAttempts": 8,
        "resetJackpotOnFailedLogon": true,
        "animationDurationMs": 2500,
        "jackpotSequenceMs": 3000,
        "respectReducedMotion": true,
        "soundEnabled": true,
        "logonSoundEnabled": false,
        "volume": 0.6,
        "demoMode": false,
        "debugMode": false,
        "showInRemoteSessions": false,
        "experimentalFilterMode": false,
        "allowEmergencyBypass": true
    })";

    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString(kJson, &warnings);

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(config.guaranteedJackpotAfterAttempts, 8);
    EXPECT_DOUBLE_EQ(config.volume, 0.6);
    EXPECT_EQ(config.jackpotSymbol, Symbol::Waffle);
    EXPECT_EQ(config.reels, 3);
}

TEST(ConfigTest, MalformedJsonFallsBackToDefaults) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString("{not valid json", &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_EQ(config.guaranteedJackpotAfterAttempts, Config::Default().guaranteedJackpotAfterAttempts);
    EXPECT_EQ(config.reelWeights, Config::Default().reelWeights);
}

TEST(ConfigTest, EmptyStringFallsBackToDefaults) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString("", &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_EQ(config.enabled, Config::Default().enabled);
}

TEST(ConfigTest, NonObjectRootFallsBackToDefaults) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString("[1, 2, 3]", &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_EQ(config.guaranteedJackpotAfterAttempts, Config::Default().guaranteedJackpotAfterAttempts);
}

TEST(ConfigTest, OutOfRangeNumbersAreClamped) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString(
        R"({"schemaVersion":1,"guaranteedJackpotAfterAttempts":9999,"volume":42.0,
            "animationDurationMs":-500})",
        &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_EQ(config.guaranteedJackpotAfterAttempts, 50);  // clamped to max
    EXPECT_DOUBLE_EQ(config.volume, 1.0);                  // clamped to max
    EXPECT_EQ(config.animationDurationMs, 0);              // clamped to min
}

TEST(ConfigTest, GuaranteedJackpotBelowMinimumIsClamped) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString(
        R"({"schemaVersion":1,"guaranteedJackpotAfterAttempts":0})", &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_EQ(config.guaranteedJackpotAfterAttempts, 1);  // clamped to min
}

TEST(ConfigTest, UnknownFieldsAreIgnoredWithoutError) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString(
        R"({"schemaVersion":1,"totallyMadeUpField":"???","reelWeights":{"NOT_A_SYMBOL":5}})", &warnings);

    // Unknown top-level field is silently ignored (no warning needed for
    // that -- only reelWeights keys are validated symbol names); the
    // unknown reelWeights key produces a warning but defaults survive.
    EXPECT_EQ(config.reelWeights, Config::Default().reelWeights);
}

// Spec §7 / §11: "jackpotSymbol: 'SEVEN'" must not be honored -- v1 pins
// jackpotSymbol to WAFFLE regardless of what the file asks for.
TEST(ConfigTest, JackpotSymbolSevenIsRejected) {
    std::vector<std::string> warnings;
    const Config config =
        ConfigLoader::LoadFromString(R"({"schemaVersion":1,"jackpotSymbol":"SEVEN"})", &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_EQ(config.jackpotSymbol, Symbol::Waffle);
}

TEST(ConfigTest, ReelsOtherThanThreeIsRejected) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString(R"({"schemaVersion":1,"reels":5})", &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_EQ(config.reels, 3);
}

TEST(ConfigTest, UnsupportedSchemaVersionFallsBackToDefaults) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString(R"({"schemaVersion":2,"volume":0.1})", &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_DOUBLE_EQ(config.volume, Config::Default().volume);
}

TEST(ConfigTest, AllowEmergencyBypassFalseIsNeverHonored) {
    std::vector<std::string> warnings;
    const Config config =
        ConfigLoader::LoadFromString(R"({"schemaVersion":1,"allowEmergencyBypass":false})", &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_TRUE(config.allowEmergencyBypass);
}

TEST(ConfigTest, AllZeroReelWeightsFallsBackToDefaultWeights) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromString(
        R"({"schemaVersion":1,"reelWeights":{"WAFFLE":0,"CHERRY":0,"LEMON":0,"BELL":0,
                                              "DIAMOND":0,"STAR":0,"SEVEN":0,"LUCKY":0}})",
        &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_EQ(config.reelWeights, Config::Default().reelWeights);
}

TEST(ConfigTest, MissingFileFallsBackToDefaults) {
    std::vector<std::string> warnings;
    const Config config = ConfigLoader::LoadFromFile("this/path/does/not/exist.json", &warnings);

    EXPECT_FALSE(warnings.empty());
    EXPECT_EQ(config.guaranteedJackpotAfterAttempts, Config::Default().guaranteedJackpotAfterAttempts);
}

TEST(ConfigTest, RoundTripsThroughToJsonString) {
    const Config original = Config::Default();
    const std::string json = ConfigLoader::ToJsonString(original);

    std::vector<std::string> warnings;
    const Config roundTripped = ConfigLoader::LoadFromString(json, &warnings);

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(roundTripped.reelWeights, original.reelWeights);
    EXPECT_EQ(roundTripped.guaranteedJackpotAfterAttempts, original.guaranteedJackpotAfterAttempts);
    EXPECT_DOUBLE_EQ(roundTripped.volume, original.volume);
}

TEST(ConfigTest, NullWarningsPointerIsSafe) {
    EXPECT_NO_THROW(ConfigLoader::LoadFromString("{not json", nullptr));
    EXPECT_NO_THROW(ConfigLoader::LoadFromString(R"({"schemaVersion":1})", nullptr));
}
