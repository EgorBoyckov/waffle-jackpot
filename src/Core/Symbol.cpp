#include "Symbol.h"

namespace waffle {

const char* ToString(Symbol symbol) noexcept {
    switch (symbol) {
        case Symbol::Waffle:  return "WAFFLE";
        case Symbol::Cherry:  return "CHERRY";
        case Symbol::Lemon:   return "LEMON";
        case Symbol::Bell:    return "BELL";
        case Symbol::Diamond: return "DIAMOND";
        case Symbol::Star:    return "STAR";
        case Symbol::Seven:   return "SEVEN";
        case Symbol::Lucky:   return "LUCKY";
        case Symbol::Count:   break;
    }
    return "UNKNOWN";
}

}  // namespace waffle
