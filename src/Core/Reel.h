#pragma once

#include <array>

#include "Rng.h"
#include "Symbol.h"

namespace waffle {

// A single reel: given its weights, draws one symbol. Deliberately has no
// concept of animation, position or timing — the render layer decides how
// to spin visually to a symbol this class already picked (spec §2.1: the
// outcome is generated before the animation, never the other way around).
class Reel {
public:
    explicit Reel(std::array<int, kSymbolCount> weights);

    Symbol Draw(Rng& rng) const;

    const std::array<int, kSymbolCount>& Weights() const { return weights_; }
    void SetWeights(std::array<int, kSymbolCount> weights) { weights_ = weights; }

private:
    std::array<int, kSymbolCount> weights_;
};

}  // namespace waffle
