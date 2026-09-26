#include <gtest/gtest.h>

#include <string>

#include "Core/Symbol.h"

using waffle::IsJackpot;
using waffle::Symbol;
using waffle::kSymbolCount;

// Spec §11: "все 512 комбинаций перебором — ровно одна true".
TEST(IsJackpotTest, ExhaustiveExactlyOneTrue) {
    int trueCount = 0;
    for (std::size_t a = 0; a < kSymbolCount; ++a) {
        for (std::size_t b = 0; b < kSymbolCount; ++b) {
            for (std::size_t c = 0; c < kSymbolCount; ++c) {
                const auto sa = static_cast<Symbol>(a);
                const auto sb = static_cast<Symbol>(b);
                const auto sc = static_cast<Symbol>(c);
                if (IsJackpot(sa, sb, sc)) {
                    ++trueCount;
                    EXPECT_EQ(sa, Symbol::Waffle);
                    EXPECT_EQ(sb, Symbol::Waffle);
                    EXPECT_EQ(sc, Symbol::Waffle);
                }
            }
        }
    }
    EXPECT_EQ(trueCount, 1);
}

TEST(IsJackpotTest, ExplicitCasesFromSpec) {
    EXPECT_TRUE(IsJackpot(Symbol::Waffle, Symbol::Waffle, Symbol::Waffle));   // WWW -> true
    EXPECT_FALSE(IsJackpot(Symbol::Waffle, Symbol::Waffle, Symbol::Cherry));  // WWC -> false
    EXPECT_FALSE(IsJackpot(Symbol::Waffle, Symbol::Cherry, Symbol::Waffle));  // WCW -> false
    EXPECT_FALSE(IsJackpot(Symbol::Cherry, Symbol::Waffle, Symbol::Waffle));  // CWW -> false
    EXPECT_FALSE(IsJackpot(Symbol::Seven, Symbol::Seven, Symbol::Seven));    // 777 -> false
    EXPECT_FALSE(IsJackpot(Symbol::Cherry, Symbol::Cherry, Symbol::Cherry)); // CCC -> false
}

TEST(IsJackpotTest, IsConstexpr) {
    constexpr bool jackpot = IsJackpot(Symbol::Waffle, Symbol::Waffle, Symbol::Waffle);
    static_assert(jackpot, "IsJackpot must be usable in a constant expression");
    EXPECT_TRUE(jackpot);
}

TEST(SymbolTest, ToStringNeverNull) {
    for (std::size_t i = 0; i < kSymbolCount; ++i) {
        const char* name = waffle::ToString(static_cast<Symbol>(i));
        ASSERT_NE(name, nullptr);
        EXPECT_GT(std::string(name).size(), 0u);
    }
}
