#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "Symbol.h"

namespace waffle {

// Abstracts where randomness comes from, so SlotMachine and Reel never call
// a system RNG API directly. This is what makes the generator testable per
// project spec §2.5: production uses SystemRandomSource, tests use
// DeterministicRandomSource with a fixed seed for reproducible outcomes.
class IRandomSource {
public:
    virtual ~IRandomSource() = default;
    virtual std::uint64_t NextUInt64() = 0;
};

// Production randomness. Seeds a std::mt19937_64 from the platform CSPRNG
// (BCryptGenRandom on Windows) once at construction, then draws from the
// PRNG — cryptographic strength is not required here (this is a joke
// project, not a security boundary; see Symbol.h / IsJackpot), only a
// well-distributed, fast source. On non-Windows platforms (e.g. this
// repository's Linux-based CI, which builds and tests WaffleCore since it
// has no Win32 dependency) it falls back to std::random_device for seeding.
class SystemRandomSource final : public IRandomSource {
public:
    SystemRandomSource();
    ~SystemRandomSource() override;
    std::uint64_t NextUInt64() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Deterministic randomness for unit tests: same seed, same sequence, every
// time. Never used in production code paths.
class DeterministicRandomSource final : public IRandomSource {
public:
    explicit DeterministicRandomSource(std::uint64_t seed);
    ~DeterministicRandomSource() override;
    std::uint64_t NextUInt64() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Weighted symbol draws on top of an IRandomSource. Owns nothing about
// jackpot/pity-timer policy — that lives in SlotMachine. This class only
// answers "given these weights, which symbol landed".
class Rng {
public:
    explicit Rng(std::unique_ptr<IRandomSource> source);

    // weights.size() == kSymbolCount, indexed by static_cast<size_t>(Symbol).
    // A weight of 0 means that symbol never appears. All weights are
    // expected non-negative with at least one positive weight; behavior is
    // undefined (defensively: returns Symbol::Waffle) if all weights are 0,
    // which Config validation is responsible for preventing.
    Symbol PickWeighted(const std::array<int, kSymbolCount>& weights);

    // Uniform draw in [0.0, 1.0), used for near-miss biasing decisions that
    // aren't plain weighted symbol picks.
    double NextUniform01();

private:
    std::unique_ptr<IRandomSource> source_;
};

}  // namespace waffle
