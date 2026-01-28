#pragma once

#include "card.h"
#include <cstdint>

namespace sharkwave {

enum class HandRank : uint8_t {
    HighCard,
    OnePair,
    TwoPair,
    ThreeOfAKind,
    Straight,
    Flush,
    FullHouse,
    FourOfAKind,
    StraightFlush,
    RoyalFlush
};

struct HandResult {
    HandRank rank;
    uint64_t value; // For tiebreaking

    constexpr auto operator<=>(const HandResult& other) const {
        if (rank != other.rank) {
            return rank <=> other.rank;
        }
        return value <=> other.value;
    }

    bool operator==(const HandResult& other) const {
        return rank == other.rank && value == other.value;
    }
};

class HandEvaluator {
public:
    // Evaluate best 5-card hand from up to 7 cards
    static HandResult evaluate(const CardSet& cards);
    static HandResult evaluate(const Card* cards, size_t count);

    // Get string representation of hand rank
    static std::string rankToString(HandRank rank);

    // Check for specific draws
    static bool hasFlushDraw(const CardSet& holeCards, const CardSet& board);
    static bool hasOpenEndedStraightDraw(const CardSet& holeCards, const CardSet& board);
    static bool hasGutshotStraightDraw(const CardSet& holeCards, const CardSet& board);
    static int countOuts(const CardSet& holeCards, const CardSet& board);

    // Calculate equity vs random hand (Monte Carlo)
    static double calculateEquity(const CardSet& holeCards, const CardSet& board,
                                  int iterations = 1000);

private:
    static bool isFlush(const Card* cards, size_t count, Suit& flushSuit);
    static bool isStraight(const Card* cards, size_t count, uint64_t& straightValue);
    static uint64_t scoreHand(const Card* cards, size_t count, HandRank& rank);
};

} // namespace sharkwave
