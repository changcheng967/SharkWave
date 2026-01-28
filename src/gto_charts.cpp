#include "gto_charts.h"
#include <algorithm>

namespace sharkwave {

namespace {
    constexpr int rankToInt(Rank r) { return static_cast<int>(r); }

    // UTG opening range: ~15% - 77+, ATs+, KQs, AJo+, KQo
    bool inUTGRange(Card c1, Card c2) {
        int r1 = rankToInt(c1.rank());
        int r2 = rankToInt(c2.rank());
        bool suited = (c1.suit() == c2.suit());
        bool paired = (r1 == r2);
        int high = std::max(r1, r2);
        int low = std::min(r1, r2);

        // Pairs 77+
        if (paired && r1 >= 7) return true;

        // High suited
        if (suited) {
            if (high == 14 && low >= 10) return true; // ATs+
            if (high == 13 && low == 12) return true; // KQs
            if (high == 12 && low == 11) return true; // QJs
        }

        // High offsuit
        if (!suited) {
            if (high == 14 && low >= 11) return true; // AJo+
            if (high == 13 && low == 12) return true; // KQo
        }

        return false;
    }

    // MP opening range: ~19% - 66+, ATs+, KJs+, QJs, AJo+, KQo
    bool inMPRange(Card c1, Card c2) {
        int r1 = rankToInt(c1.rank());
        int r2 = rankToInt(c2.rank());
        bool suited = (c1.suit() == c2.suit());
        bool paired = (r1 == r2);
        int high = std::max(r1, r2);
        int low = std::min(r1, r2);

        // Pairs 66+
        if (paired && r1 >= 6) return true;

        // Suited
        if (suited) {
            if (high == 14 && low >= 9) return true;  // A9s+
            if (high == 13 && low >= 10) return true; // KTs+
            if (high == 12 && low >= 11) return true; // QJs+
        }

        // Offsuit
        if (!suited) {
            if (high == 14 && low >= 10) return true; // ATo+
            if (high == 13 && low >= 11) return true; // KJo+
            if (high == 12 && low == 11) return true; // QJo
        }

        return false;
    }

    // CO opening range: ~28% - 55+, ATs+, KTs+, QJs+, JTs, ATo+, KJo+, QJo
    bool inCORange(Card c1, Card c2) {
        int r1 = rankToInt(c1.rank());
        int r2 = rankToInt(c2.rank());
        bool suited = (c1.suit() == c2.suit());
        bool paired = (r1 == r2);
        int high = std::max(r1, r2);
        int low = std::min(r1, r2);

        // Pairs 55+
        if (paired && r1 >= 5) return true;

        // Suited - THIS WAS THE BUG: JTc should raise
        if (suited) {
            if (high == 14 && low >= 7) return true;  // A7s+
            if (high == 13 && low >= 8) return true;  // K8s+
            if (high == 12 && low >= 9) return true;  // Q9s+
            if (high == 11 && low >= 9) return true;  // J9s+
            if (high == 10 && low >= 9) return true;  // T9s
        }

        // Offsuit
        if (!suited) {
            if (high == 14 && low >= 9) return true;  // A9o+
            if (high == 13 && low >= 10) return true; // KTo+
            if (high == 12 && low >= 10) return true; // QTo+
            if (high == 11 && low >= 10) return true; // JTo
        }

        return false;
    }

    // BTN opening range: ~45% - very wide
    bool inBTNRange(Card c1, Card c2) {
        int r1 = rankToInt(c1.rank());
        int r2 = rankToInt(c2.rank());
        bool suited = (c1.suit() == c2.suit());
        bool paired = (r1 == r2);
        int high = std::max(r1, r2);
        int low = std::min(r1, r2);

        // All pairs
        if (paired) return true;

        // Suited - almost all suited connectors and gappers
        if (suited) {
            if (high == 14 && low >= 2) return true;  // All Ax suited
            if (high == 13 && low >= 5) return true;  // K5s+
            if (high == 12 && low >= 6) return true;  // Q6s+
            if (high >= 9 && low >= 5) return true;   // J9s+, T9s, 98s
            if (high - low <= 3 && high >= 7) return true; // Connected 1-3 gap
        }

        // Offsuit - more selective
        if (!suited) {
            if (high == 14 && low >= 5) return true;  // A5o+
            if (high == 13 && low >= 8) return true;  // K8o+
            if (high == 12 && low >= 9) return true;  // Q9o+
            if (high == 11 && low >= 9) return true;  // J9o+
            if (high >= 9 && high - low <= 1) return true; // JTo, T9o
        }

        return false;
    }

    // SB opening range: ~38% - wide but cautious of BB
    bool inSBRange(Card c1, Card c2) {
        int r1 = rankToInt(c1.rank());
        int r2 = rankToInt(c2.rank());
        bool suited = (c1.suit() == c2.suit());
        bool paired = (r1 == r2);
        int high = std::max(r1, r2);
        int low = std::min(r1, r2);

        // All pairs
        if (paired) return true;

        // Suited - strong suited hands
        if (suited) {
            if (high == 14 && low >= 4) return true;  // A4s+
            if (high == 13 && low >= 6) return true;  // K6s+
            if (high == 12 && low >= 7) return true;  // Q7s+
            if (high == 11 && low >= 8) return true;  // J8s+
            if (high == 10 && low >= 8) return true;  // T8s+
        }

        // Offsuit
        if (!suited) {
            if (high == 14 && low >= 7) return true;  // A7o+
            if (high == 13 && low >= 9) return true;  // K9o+
            if (high == 12 && low >= 9) return true;  // Q9o+
            if (high == 11 && low >= 9) return true;  // J9o
        }

        return false;
    }
}

GtoDecision GtoCharts::getAction(Position pos, const CardSet& holeCards,
                                 int bigBlinds, bool facingRaise) {
    if (holeCards.count < 2) {
        return {GtoAction::Fold, 0};
    }

    Card c1 = holeCards.cards[0];
    Card c2 = holeCards.cards[1];

    // Short stack - shove or fold
    if (bigBlinds < 25) {
        if (isPremium(c1, c2) || isBrodier(c1, c2) || isSpeculative(c1, c2)) {
            return {GtoAction::AllIn, 0};
        }
        return {GtoAction::Fold, 0};
    }

    if (facingRaise) {
        // 3-bet or call logic
        if (should3bet(pos, c1, c2)) {
            return {GtoAction::Raise, static_cast<int>(bigBlinds * 2.5)};
        }
        if (shouldCall3bet(pos, c1, c2)) {
            return {GtoAction::Call, 0};
        }
        return {GtoAction::Fold, 0};
    }

    // Unopened pot - check opening range
    if (shouldOpen(pos, c1, c2)) {
        int raiseSize = 25; // 2.5x default
        if (pos == Position::SB) raiseSize = 20;
        return {GtoAction::Raise, raiseSize};
    }

    return {GtoAction::Fold, 0};
}

bool GtoCharts::shouldOpen(Position pos, Card c1, Card c2) {
    switch (pos) {
        case Position::UTG: return inUTGRange(c1, c2);
        case Position::MP:  return inMPRange(c1, c2);
        case Position::CO:  return inCORange(c1, c2);
        case Position::BTN: return inBTNRange(c1, c2);
        case Position::SB:  return inSBRange(c1, c2);
        case Position::BB:  return false; // Can't open from BB
    }
    return false;
}

bool GtoCharts::should3bet(Position pos, Card c1, Card c2) {
    (void)pos;
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    bool suited = (c1.suit() == c2.suit());
    bool paired = (r1 == r2);
    int high = std::max(r1, r2);

    // 3-bet range: QQ+, AK, AQs
    if (paired && r1 >= 12) return true; // QQ+
    if (high == 14 && r2 == 13) return true; // AK
    if (suited && high == 14 && r2 >= 12) return true; // AQs+
    if (suited && high == 13 && r2 == 12) return true; // KQs

    return false;
}

bool GtoCharts::should4bet(Position pos, Card c1, Card c2) {
    (void)pos;
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    bool paired = (r1 == r2);

    // 4-bet range: KK+, AKs
    if (paired && r1 >= 13) return true; // KK+
    if (r1 == 14 && r2 == 13 && c1.suit() == c2.suit()) return true; // AKs

    return false;
}

bool GtoCharts::shouldCall3bet(Position pos, Card c1, Card c2) {
    (void)pos;
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    bool suited = (c1.suit() == c2.suit());
    bool paired = (r1 == r2);
    int high = std::max(r1, r2);

    // Call 3-bet: TT-99, AQs-AJs, KQs, T9s-98s
    if (paired && r1 >= 9 && r1 <= 11) return true; // 99-JJ
    if (suited && high == 14 && r2 >= 11 && r2 <= 12) return true; // AQs-AJs
    if (suited && high == 13 && r2 == 12) return true; // KQs
    if (suited && high >= 9 && (high - r2) <= 1) return true; // Suited connectors 98+

    return false;
}

int GtoCharts::handType(Card c1, Card c2) {
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    if (r1 == r2) return 0; // pair
    if (c1.suit() == c2.suit()) return 1; // suited
    return 2; // offsuit
}

bool GtoCharts::isPremium(Card c1, Card c2) {
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    bool paired = (r1 == r2);
    int high = std::max(r1, r2);

    // AA, KK, QQ, AK
    if (paired && r1 >= 12) return true; // QQ+
    if (high == 14 && r2 == 13) return true; // AK

    return false;
}

bool GtoCharts::isBrodier(Card c1, Card c2) {
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    bool paired = (r1 == r2);
    bool suited = (c1.suit() == c2.suit());
    int high = std::max(r1, r2);

    // JJ, TT, AQs, KQs
    if (paired && (r1 == 11 || r1 == 10)) return true;
    if (suited && high == 14 && r2 == 12) return true; // AQs
    if (suited && high == 13 && r2 == 12) return true; // KQs

    return false;
}

bool GtoCharts::isSpeculative(Card c1, Card c2) {
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    bool suited = (c1.suit() == c2.suit());
    bool paired = (r1 == r2);
    int high = std::max(r1, r2);
    int low = std::min(r1, r2);

    // Small pairs, suited connectors, suited aces
    if (paired && r1 <= 9) return true; // 22-99
    if (suited && high <= 13 && (high - low) <= 2 && high >= 7) return true; // Suited connectors
    if (suited && high == 14 && low <= 9) return true; // Suited aces

    return false;
}

uint16_t GtoCharts::handToIndex(Card c1, Card c2) {
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    bool suited = (c1.suit() == c2.suit());

    if (r1 < r2) std::swap(r1, r2);

    uint16_t type = suited ? 1 : (r1 == r2 ? 0 : 2);
    return static_cast<uint16_t>(r1 * 100 + r2 * 2 + type);
}

// Stub implementations for remaining functions
bool GtoCharts::inRfiRange(Position pos, uint16_t handIdx) {
    (void)pos;
    (void)handIdx;
    return false;
}

bool GtoCharts::in3betRange(Position pos, uint16_t handIdx) {
    (void)pos;
    (void)handIdx;
    return false;
}

bool GtoCharts::in4betRange(Position pos, uint16_t handIdx) {
    (void)pos;
    (void)handIdx;
    return false;
}

bool GtoCharts::inCall3betRange(Position pos, uint16_t handIdx) {
    (void)pos;
    (void)handIdx;
    return false;
}

} // namespace sharkwave
