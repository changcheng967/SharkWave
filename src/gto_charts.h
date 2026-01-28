#pragma once

#include "card.h"
#include "game_session.h"

namespace sharkwave {

enum class GtoAction : uint8_t {
    Fold,
    Call,
    Raise,
    AllIn
};

struct GtoDecision {
    GtoAction action;
    int raiseSize; // as multiple of BB, 0 for N/A
};

class GtoCharts {
public:
    // Get GTO action for a given situation
    static GtoDecision getAction(Position pos, const CardSet& holeCards,
                                 int bigBlinds, bool facingRaise);

    // Preflop opening ranges by position
    static bool shouldOpen(Position pos, Card c1, Card c2);
    static bool should3bet(Position pos, Card c1, Card c2);
    static bool should4bet(Position pos, Card c1, Card c2);
    static bool shouldCall3bet(Position pos, Card c1, Card c2);

    // Hand strength utilities
    static int handType(Card c1, Card c2);
    static bool isPremium(Card c1, Card c2);
    static bool isBrodier(Card c1, Card c2);
    static bool isSpeculative(Card c1, Card c2);

private:
    // Convert hand to index for lookup table
    static uint16_t handToIndex(Card c1, Card c2);

    // Range definitions (simplified for MVP)
    static bool inRfiRange(Position pos, uint16_t handIdx);
    static bool in3betRange(Position pos, uint16_t handIdx);
    static bool in4betRange(Position pos, uint16_t handIdx);
    static bool inCall3betRange(Position pos, uint16_t handIdx);
};

} // namespace sharkwave
