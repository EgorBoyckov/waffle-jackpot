#include "Rng.h"

#include <algorithm>
#include <random>

#if defined(_WIN32)
#include <bcrypt.h>
#include <windows.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace waffle {
namespace {

// Adapts IRandomSource to the UniformRandomBitGenerator concept so it can
// back std::discrete_distribution / std::uniform_real_distribution without
// those distributions knowing anything about BCrypt or mt19937.
class RandomSourceEngine {
public:
    using result_type = std::uint64_t;

    explicit RandomSourceEngine(IRandomSource& source) : source_(source) {}

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT64_MAX; }
    result_type operator()() { return source_.NextUInt64(); }

private:
    IRandomSource& source_;
};

std::uint64_t SeedFromOsEntropy() {
#if defined(_WIN32)
    std::uint64_t seed = 0;
    NTSTATUS status = BCryptGenRandom(
        nullptr, reinterpret_cast<PUCHAR>(&seed), sizeof(seed),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status >= 0) {
        return seed;
    }
    // Fall through to a non-cryptographic fallback; this is a joke project's
    // RNG seed, not a security boundary (see Symbol.h), so a degraded seed
    // is acceptable and must never crash LogonUI.
    return static_cast<std::uint64_t>(GetTickCount64()) ^ 0x9E3779B97F4A7C15ULL;
#else
    // Non-Windows build (this repository's Linux CI builds and tests
    // WaffleCore, which has no Win32 dependency by design).
    std::random_device rd;
    std::uint64_t seed = (static_cast<std::uint64_t>(rd()) << 32) | rd();
    return seed;
#endif
}

}  // namespace

struct SystemRandomSource::Impl {
    std::mt19937_64 engine{SeedFromOsEntropy()};
};

SystemRandomSource::SystemRandomSource() : impl_(std::make_unique<Impl>()) {}
SystemRandomSource::~SystemRandomSource() = default;

std::uint64_t SystemRandomSource::NextUInt64() {
    return impl_->engine();
}

struct DeterministicRandomSource::Impl {
    explicit Impl(std::uint64_t seed) : engine(seed) {}
    std::mt19937_64 engine;
};

DeterministicRandomSource::DeterministicRandomSource(std::uint64_t seed)
    : impl_(std::make_unique<Impl>(seed)) {}
DeterministicRandomSource::~DeterministicRandomSource() = default;

std::uint64_t DeterministicRandomSource::NextUInt64() {
    return impl_->engine();
}

Rng::Rng(std::unique_ptr<IRandomSource> source) : source_(std::move(source)) {}

Symbol Rng::PickWeighted(const std::array<int, kSymbolCount>& weights) {
    const bool anyPositive = std::any_of(weights.begin(), weights.end(),
                                          [](int w) { return w > 0; });
    if (!anyPositive) {
        // Config validation should never let this happen; defensively
        // degrade to WAFFLE rather than feed discrete_distribution an
        // all-zero weight set (undefined behavior).
        return Symbol::Waffle;
    }

    RandomSourceEngine engine(*source_);
    std::discrete_distribution<std::size_t> dist(weights.begin(), weights.end());
    return static_cast<Symbol>(dist(engine));
}

double Rng::NextUniform01() {
    RandomSourceEngine engine(*source_);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(engine);
}

}  // namespace waffle
