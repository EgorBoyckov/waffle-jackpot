#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace waffle {

// Slot machine reel symbols. WAFFLE is the only symbol that matters for
// IsJackpot(); the rest exist to make three-in-a-row rare and to give the
// near-miss logic something to almost-match.
enum class Symbol : std::uint8_t {
    Waffle = 0,
    Cherry,
    Lemon,
    Bell,
    Diamond,
    Star,
    Seven,
    Lucky,
    Count  // sentinel, not a real symbol
};

inline constexpr std::size_t kSymbolCount = static_cast<std::size_t>(Symbol::Count);

// The one rule of the whole project (see project spec §1): three waffles in
// a row, nothing else. Everything else in this codebase exists to control
// how often this becomes true, never to change what "true" means.
constexpr bool IsJackpot(Symbol a, Symbol b, Symbol c) noexcept {
    return a == Symbol::Waffle && b == Symbol::Waffle && c == Symbol::Waffle;
}

// Human-readable name, e.g. for the debug overlay and logs. Never used for
// anything security-relevant.
const char* ToString(Symbol symbol) noexcept;

}  // namespace waffle
