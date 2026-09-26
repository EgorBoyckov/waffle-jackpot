#include "Reel.h"

namespace waffle {

Reel::Reel(std::array<int, kSymbolCount> weights) : weights_(weights) {}

Symbol Reel::Draw(Rng& rng) const {
    return rng.PickWeighted(weights_);
}

}  // namespace waffle
