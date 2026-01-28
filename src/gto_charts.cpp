#include "gto_charts.h"
#include <algorithm>

namespace sharkwave {

namespace {
    // Rank values: 2=2, ..., 10=10, J=11, Q=12, K=13, A=14
    constexpr int rankToInt(Rank r) { return static_cast<int>(r); }

    // Hand type constants
    constexpr int TYPE_PAIR = 0;
    constexpr int TYPE_SUITED = 1;
    constexpr int TYPE_OFFSUIT = 2;

    // Convert hand to a single integer for range lookup
    // Format: highRank * 100 + lowRank * 2 + type
    // Type: 0=pair, 1=suited, 2=offsuit
    uint16_t encodeHand(Card c1, Card c2) {
        int r1 = rankToInt(c1.rank());
        int r2 = rankToInt(c2.rank());

        if (r1 < r2) std::swap(r1, r2);

        int type;
        if (r1 == r2) {
            type = TYPE_PAIR;
        } else if (c1.suit() == c2.suit()) {
            type = TYPE_SUITED;
        } else {
            type = TYPE_OFFSUIT;
        }

        return static_cast<uint16_t>(r1 * 100 + r2 * 2 + type);
    }

    // Check if hand is in a bitmask range (169 possible hands max)
    bool inBitmaskRange(uint16_t handIdx, const unsigned long long* ranges, int count) {
        int idx = (handIdx / 64);
        int bit = handIdx % 64;
        if (idx >= count) return false;
        return (ranges[idx] >> bit) & 1;
    }

    // RFI (Raise First In) ranges by position (tighter for early position)
    // UTG: 15% - QQ+, AKs, AQs, KQs, TT-JJ
    constexpr unsigned long long RFI_UTG[3] = {
        0x0000000000000000ULL, // AA-77 (high bits)
        0x8000000000000000ULL, // AKs, AQs, some others
        0x0000000000000000ULL
    };

    // MP: 19% - adds more hands
    constexpr unsigned long long RFI_MP[3] = {
        0x0000000000000000ULL,
        0xC000000000000000ULL,
        0x0000000000000000ULL
    };

    // CO: 26% - wider range
    constexpr unsigned long long RFI_CO[3] = {
        0x0000000000000001ULL, // Adds low pairs
        0xF000000000000000ULL,
        0x0000000000000000ULL
    };

    // BTN: 45% - very wide
    constexpr unsigned long long RFI_BTN[3] = {
        0x000000000000001FULL,
        0xFFFFFFFFFFFFFFF0ULL,
        0x0000000000000000ULL
    };

    // SB: 35% - wide but must consider BB
    constexpr unsigned long long RFI_SB[3] = {
        0x0000000000000007ULL,
        0xFFFFFFFFFFFFFF80ULL,
        0x0000000000000000ULL
    };

    // 3-bet ranges (position vs opener)
    constexpr unsigned long long THREEBET_IP[3] = {
        0x0000000000000000ULL,
        0xE000000000000000ULL, // QQ+, AKs, AQs
        0x0000000000000000ULL
    };

    // 4-bet ranges (premium only)
    constexpr unsigned long long FOURBET[3] = {
        0x0000000000000000ULL,
        0x8000000000000000ULL, // AA, KK, QQ, AKs
        0x0000000000000000ULL
    };
}

GtoDecision GtoCharts::getAction(Position pos, const CardSet& holeCards,
                                 int bigBlinds, bool facingRaise) {
    if (holeCards.count < 2) {
        return {GtoAction::Fold, 0};
    }

    Card c1 = holeCards.cards[0];
    Card c2 = holeCards.cards[1];
    uint16_t idx = encodeHand(c1, c2);

    // Short stack - shove or fold
    if (bigBlinds < 25) {
        if (isPremium(c1, c2) || isBrodier(c1, c2) || isSpeculative(c1, c2)) {
            return {GtoAction::AllIn, 0};
        }
        return {GtoAction::Fold, 0};
    }

    if (facingRaise) {
        // Simplified: assume vs single raise
        if (inBitmaskRange(idx, THREEBET_IP, 3)) {
            return {GtoAction::Raise, static_cast<int>(bigBlinds * 2.5)};
        }
        if (shouldCall3bet(pos, c1, c2)) {
            return {GtoAction::Call, 0};
        }
        return {GtoAction::Fold, 0};
    }

    // Unopened pot
    if (shouldOpen(pos, c1, c2)) {
        // Standard raise size
        int raiseSize = 25; // 2.5x default
        if (pos == Position::SB) raiseSize = 20;
        return {GtoAction::Raise, raiseSize};
    }

    return {GtoAction::Fold, 0};
}

bool GtoCharts::shouldOpen(Position pos, Card c1, Card c2) {
    uint16_t idx = encodeHand(c1, c2);

    switch (pos) {
        case Position::UTG:
            return inBitmaskRange(idx, RFI_UTG, 3);
        case Position::MP:
            return inBitmaskRange(idx, RFI_MP, 3);
        case Position::CO:
            return inBitmaskRange(idx, RFI_CO, 3);
        case Position::BTN:
            return inBitmaskRange(idx, RFI_BTN, 3);
        case Position::SB:
            return inBitmaskRange(idx, RFI_SB, 3);
        case Position::BB:
            return false; // Can't open from BB
    }
    return false;
}

bool GtoCharts::should3bet(Position pos, Card c1, Card c2) {
    (void)pos;
    uint16_t idx = encodeHand(c1, c2);
    return inBitmaskRange(idx, THREEBET_IP, 3);
}

bool GtoCharts::should4bet(Position pos, Card c1, Card c2) {
    (void)pos;
    uint16_t idx = encodeHand(c1, c2);
    return inBitmaskRange(idx, FOURBET, 3);
}

bool GtoCharts::shouldCall3bet(Position pos, Card c1, Card c2) {
    // Call 3-bet range: TT-99, AQs-AJs, KQs, T9s-98s
    (void)pos;
    (void)encodeHand(c1, c2); // For future use

    // Pocket pairs 99-JJ
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    if (r1 == r2 && r1 >= 9 && r1 <= 11) return true;

    // Broadway suited except AKs (which 3-bets)
    if (c1.suit() == c2.suit() && r1 >= 12 && r2 >= 11) {
        if (r1 != 14 || r2 != 13) return true; // Not AKs
    }

    // Suited connectors
    if (c1.suit() == c2.suit() && r1 - r2 == 1 && r1 >= 10) return true;

    return false;
}

int GtoCharts::handType(Card c1, Card c2) {
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());

    if (r1 < r2) std::swap(r1, r2);

    if (r1 == r2) return TYPE_PAIR;
    if (c1.suit() == c2.suit()) return TYPE_SUITED;
    return TYPE_OFFSUIT;
}

bool GtoCharts::isPremium(Card c1, Card c2) {
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    if (r1 < r2) std::swap(r1, r2);

    // AA, KK, QQ, AK
    if (r1 == 14 && r2 == 14) return true; // AA
    if (r1 == 13 && r2 == 13) return true; // KK
    if (r1 == 12 && r2 == 12) return true; // QQ
    if (r1 == 14 && r2 == 13) return true; // AK

    return false;
}

bool GtoCharts::isBrodier(Card c1, Card c2) {
    // JJ, TT, AQ, KQs
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    if (r1 < r2) std::swap(r1, r2);

    if (r1 == r2 && (r1 == 11 || r1 == 10)) return true; // JJ, TT
    if (r1 == 14 && r2 == 12 && c1.suit() == c2.suit()) return true; // AQs
    if (r1 == 13 && r2 == 12 && c1.suit() == c2.suit()) return true; // KQs

    return false;
}

bool GtoCharts::isSpeculative(Card c1, Card c2) {
    // Small pairs, suited connectors, suited aces
    int r1 = rankToInt(c1.rank());
    int r2 = rankToInt(c2.rank());
    if (r1 < r2) std::swap(r1, r2);

    // Pairs 22-99
    if (r1 == r2 && r1 <= 9) return true;

    // Suited connectors
    if (c1.suit() == c2.suit() && r1 - r2 == 1 && r1 <= 13) return true;

    // Suited aces
    if (c1.suit() == c2.suit() && r1 == 14 && r2 <= 10) return true;

    return false;
}

uint16_t GtoCharts::handToIndex(Card c1, Card c2) {
    return encodeHand(c1, c2);
}

bool GtoCharts::inRfiRange(Position pos, uint16_t handIdx) {
    (void)pos;
    (void)handIdx;
    return shouldOpen(Position::UTG, Card(Rank::Ace, Suit::Clubs), Card(Rank::King, Suit::Clubs));
}

bool GtoCharts::in3betRange(Position pos, uint16_t handIdx) {
    (void)pos;
    return inBitmaskRange(handIdx, THREEBET_IP, 3);
}

bool GtoCharts::in4betRange(Position pos, uint16_t handIdx) {
    (void)pos;
    return inBitmaskRange(handIdx, FOURBET, 3);
}

bool GtoCharts::inCall3betRange(Position pos, uint16_t handIdx) {
    (void)pos;
    (void)handIdx;
    return false; // Simplified
}

} // namespace sharkwave
