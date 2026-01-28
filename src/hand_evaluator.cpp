#include "hand_evaluator.h"
#include <algorithm>
#include <array>
#include <format>
#include <random>

namespace sharkwave {

namespace {
    // Get numeric value of rank for comparison
    constexpr int rankValue(Rank r) {
        return static_cast<int>(r);
    }

    // Count occurrences of each rank
    std::array<int, 15> countRanks(const Card* cards, size_t count) {
        std::array<int, 15> counts{};
        for (size_t i = 0; i < count; ++i) {
            counts[rankValue(cards[i].rank())]++;
        }
        return counts;
    }

    // Count occurrences of each suit
    std::array<int, 4> countSuits(const Card* cards, size_t count) {
        std::array<int, 4> counts{};
        for (size_t i = 0; i < count; ++i) {
            counts[static_cast<int>(cards[i].suit())]++;
        }
        return counts;
    }

    // Check for flush and return the flush suit
    bool findFlushSuit(const Card* cards, size_t count, Suit& flushSuit) {
        auto suitCounts = countSuits(cards, count);
        for (int i = 0; i < 4; ++i) {
            if (suitCounts[i] >= 5) {
                flushSuit = static_cast<Suit>(i);
                return true;
            }
        }
        return false;
    }
}

HandResult HandEvaluator::evaluate(const CardSet& cards) {
    return evaluate(cards.cards, cards.count);
}

HandResult HandEvaluator::evaluate(const Card* cards, size_t count) {
    if (count < 5) {
        return {HandRank::HighCard, 0};
    }

    // Check for flush
    Suit flushSuit;
    bool hasFlush = findFlushSuit(cards, count, flushSuit);

    // Check for straight
    uint64_t straightValue = 0;
    bool hasStraight = isStraight(cards, count, straightValue);

    // Royal flush and straight flush
    if (hasFlush && hasStraight) {
        // Extract flush cards
        Card flushCards[7];
        size_t flushCount = 0;
        for (size_t i = 0; i < count; ++i) {
            if (cards[i].suit() == flushSuit) {
                flushCards[flushCount++] = cards[i];
            }
        }
        uint64_t sfValue = 0;
        if (isStraight(flushCards, flushCount, sfValue)) {
            if (sfValue >= static_cast<uint64_t>(Rank::Ten) << 48) {
                return {HandRank::RoyalFlush, sfValue};
            }
            return {HandRank::StraightFlush, sfValue};
        }
    }

    // Four of a kind
    auto rankCounts = countRanks(cards, count);
    for (int r = 14; r >= 2; --r) {
        if (rankCounts[r] == 4) {
            uint64_t value = static_cast<uint64_t>(r) << 48;
            // Find kicker
            for (int k = 14; k >= 2; --k) {
                if (k != r && rankCounts[k] > 0) {
                    value |= static_cast<uint64_t>(k) << 32;
                    break;
                }
            }
            return {HandRank::FourOfAKind, value};
        }
    }

    // Full house
    int threeRank = 0, pairRank = 0;
    for (int r = 14; r >= 2; --r) {
        if (rankCounts[r] >= 3 && threeRank == 0) {
            threeRank = r;
        } else if (rankCounts[r] >= 2 && pairRank == 0) {
            pairRank = r;
        }
    }
    if (threeRank > 0) {
        // Check for two pair to upgrade
        for (int r = 14; r >= 2; --r) {
            if (r != threeRank && rankCounts[r] >= 3) {
                pairRank = r; // Use lower three of a kind as pair
                break;
            }
        }
        if (pairRank > 0 || rankCounts[threeRank] >= 2) {
            uint64_t value = static_cast<uint64_t>(threeRank) << 48;
            value |= static_cast<uint64_t>(pairRank > 0 ? pairRank : threeRank) << 32;
            return {HandRank::FullHouse, value};
        }
    }

    // Flush
    if (hasFlush) {
        uint64_t value = 0;
        int shift = 48;
        for (int r = 14; r >= 2 && shift >= 0; --r) {
            for (size_t i = 0; i < count && shift >= 0; ++i) {
                if (cards[i].suit() == flushSuit && rankValue(cards[i].rank()) == r) {
                    value |= static_cast<uint64_t>(r) << shift;
                    shift -= 4;
                    break;
                }
            }
        }
        return {HandRank::Flush, value};
    }

    // Straight
    if (hasStraight) {
        return {HandRank::Straight, straightValue};
    }

    // Three of a kind
    for (int r = 14; r >= 2; --r) {
        if (rankCounts[r] == 3) {
            uint64_t value = static_cast<uint64_t>(r) << 48;
            int kickers = 0;
            for (int k = 14; k >= 2 && kickers < 2; --k) {
                if (k != r && rankCounts[k] > 0) {
                    value |= static_cast<uint64_t>(k) << (32 - kickers * 16);
                    kickers++;
                }
            }
            return {HandRank::ThreeOfAKind, value};
        }
    }

    // Two pair
    int pairs[2] = {0, 0};
    int pairIdx = 0;
    for (int r = 14; r >= 2 && pairIdx < 2; --r) {
        if (rankCounts[r] == 2) {
            pairs[pairIdx++] = r;
        }
    }
    if (pairIdx >= 2) {
        uint64_t value = static_cast<uint64_t>(pairs[0]) << 48;
        value |= static_cast<uint64_t>(pairs[1]) << 32;
        // Find kicker
        for (int k = 14; k >= 2; --k) {
            if (k != pairs[0] && k != pairs[1] && rankCounts[k] > 0) {
                value |= static_cast<uint64_t>(k) << 16;
                break;
            }
        }
        return {HandRank::TwoPair, value};
    }

    // One pair
    for (int r = 14; r >= 2; --r) {
        if (rankCounts[r] == 2) {
            uint64_t value = static_cast<uint64_t>(r) << 48;
            int kickers = 0;
            for (int k = 14; k >= 2 && kickers < 3; --k) {
                if (k != r && rankCounts[k] > 0) {
                    value |= static_cast<uint64_t>(k) << (32 - kickers * 12);
                    kickers++;
                }
            }
            return {HandRank::OnePair, value};
        }
    }

    // High card
    uint64_t value = 0;
    int kickers = 0;
    for (int r = 14; r >= 2 && kickers < 5; --r) {
        if (rankCounts[r] > 0) {
            value |= static_cast<uint64_t>(r) << (48 - kickers * 12);
            kickers++;
        }
    }
    return {HandRank::HighCard, value};
}

bool HandEvaluator::isStraight(const Card* cards, size_t count, uint64_t& straightValue) {
    // Collect unique ranks
    std::array<bool, 15> hasRank{};
    for (size_t i = 0; i < count; ++i) {
        hasRank[rankValue(cards[i].rank())] = true;
    }

    // Check for A-2-3-4-5 straight (wheel)
    if (hasRank[14]) hasRank[1] = true; // Ace can be low

    // Find highest straight
    for (int start = 14; start >= 5; --start) {
        bool straight = true;
        for (int i = 0; i < 5; ++i) {
            if (!hasRank[start - i]) {
                straight = false;
                break;
            }
        }
        if (straight) {
            // Return value with high card at bit 48
            straightValue = static_cast<uint64_t>(start == 1 ? 5 : start) << 48;
            return true;
        }
    }
    return false;
}

bool HandEvaluator::isFlush(const Card* cards, size_t count, Suit& flushSuit) {
    return findFlushSuit(cards, count, flushSuit);
}

std::string HandEvaluator::rankToString(HandRank rank) {
    switch (rank) {
        case HandRank::HighCard:       return "High Card";
        case HandRank::OnePair:        return "Pair";
        case HandRank::TwoPair:        return "Two Pair";
        case HandRank::ThreeOfAKind:   return "Three of a Kind";
        case HandRank::Straight:       return "Straight";
        case HandRank::Flush:          return "Flush";
        case HandRank::FullHouse:      return "Full House";
        case HandRank::FourOfAKind:    return "Four of a Kind";
        case HandRank::StraightFlush:  return "Straight Flush";
        case HandRank::RoyalFlush:     return "Royal Flush";
    }
    return "Unknown";
}

bool HandEvaluator::hasFlushDraw(const CardSet& holeCards, const CardSet& board) {
    if (board.count < 3) return false;

    CardSet combined;
    for (size_t i = 0; i < holeCards.count; ++i) combined.add(holeCards.cards[i]);
    for (size_t i = 0; i < board.count; ++i) combined.add(board.cards[i]);

    auto suitCounts = countSuits(combined.cards, combined.count);
    for (int i = 0; i < 4; ++i) {
        if (suitCounts[i] == 4) return true; // One more card needed
    }
    return false;
}

bool HandEvaluator::hasOpenEndedStraightDraw(const CardSet& holeCards, const CardSet& board) {
    CardSet combined;
    for (size_t i = 0; i < holeCards.count; ++i) combined.add(holeCards.cards[i]);
    for (size_t i = 0; i < board.count; ++i) combined.add(board.cards[i]);

    if (combined.count < 4) return false;

    std::array<bool, 15> hasRank{};
    for (size_t i = 0; i < combined.count; ++i) {
        hasRank[rankValue(combined.cards[i].rank())] = true;
    }

    // Check for 4 consecutive cards (can complete on either end)
    for (int start = 11; start >= 1; --start) {
        int count = 0;
        for (int i = 0; i < 5; ++i) {
            if (hasRank[start + i]) count++;
        }
        if (count == 4) return true;
    }

    // Special case: A-2-3-4 (can complete with 5)
    int wheelCount = hasRank[14] + hasRank[2] + hasRank[3] + hasRank[4];
    if (wheelCount == 4) return true;

    return false;
}

bool HandEvaluator::hasGutshotStraightDraw(const CardSet& holeCards, const CardSet& board) {
    if (hasOpenEndedStraightDraw(holeCards, board)) return true;

    CardSet combined;
    for (size_t i = 0; i < holeCards.count; ++i) combined.add(holeCards.cards[i]);
    for (size_t i = 0; i < board.count; ++i) combined.add(board.cards[i]);

    if (combined.count < 4) return false;

    std::array<bool, 15> hasRank{};
    for (size_t i = 0; i < combined.count; ++i) {
        hasRank[rankValue(combined.cards[i].rank())] = true;
    }

    // Check for gutshot (4 cards that need 1 specific rank in the middle)
    for (int start = 10; start >= 1; --start) {
        int consecutive = 0;
        int gap = 0;
        for (int i = 0; i < 5; ++i) {
            if (hasRank[start + i]) {
                consecutive++;
            } else if (gap == 0) {
                gap = 1;
            } else {
                break;
            }
        }
        if (consecutive == 4 && gap == 1) return true;
    }

    return false;
}

int HandEvaluator::countOuts(const CardSet& holeCards, const CardSet& board) {
    CardSet combined;
    for (size_t i = 0; i < holeCards.count; ++i) combined.add(holeCards.cards[i]);
    for (size_t i = 0; i < board.count; ++i) combined.add(board.cards[i]);

    if (combined.count < 4) return 0;

    // Get current hand strength
    HandResult currentHand = evaluate(combined);

    // Count cards that improve hand
    int outs = 0;
    std::array<bool, 15> hasRank{};
    std::array<bool, 4> hasSuitWithRank[15]{};
    std::array<bool, 52> hasCard{};

    // Build card presence map
    for (size_t i = 0; i < combined.count; ++i) {
        Rank r = combined.cards[i].rank();
        Suit s = combined.cards[i].suit();
        hasRank[rankValue(r)] = true;
        hasSuitWithRank[rankValue(r)][static_cast<int>(s)] = true;
        int cardIdx = (rankValue(r) - 2) * 4 + static_cast<int>(s);
        hasCard[cardIdx] = true;
    }

    // Check flush draw outs
    for (int s = 0; s < 4; ++s) {
        int suitCount = 0;
        for (size_t i = 0; i < combined.count; ++i) {
            if (static_cast<int>(combined.cards[i].suit()) == s) suitCount++;
        }
        if (suitCount == 4) {
            // 9 outs for flush (13 - 4 = 9)
            for (int r = 2; r <= 14; ++r) {
                if (!hasSuitWithRank[r][s]) outs++;
            }
            break;
        }
    }

    // Check straight draw outs
    std::array<bool, 15> rankHas = hasRank;
    if (rankHas[14]) rankHas[1] = true; // Ace low

    // Open-ended: 8 outs
    for (int start = 10; start >= 1; --start) {
        int count = 0;
        int missing[2] = {0, 0};
        int missingIdx = 0;
        for (int i = 0; i < 5; ++i) {
            if (rankHas[start + i]) count++;
            else if (missingIdx < 2) missing[missingIdx++] = start + i;
        }
        if (count == 3 && missingIdx == 2) {
            // Gutshot: 4 outs
            for (int m : missing) {
                int r = (m == 1) ? 14 : m;
                if (!hasRank[r]) outs++;
            }
        }
    }

    // Overcard outs (simplified)
    HandResult boardOnly = evaluate(board.cards, board.count);
    if (currentHand.rank <= HandRank::OnePair) {
        int boardTopRank = static_cast<int>(boardOnly.value >> 48);
        for (int r = 14; r >= 11; --r) {
            if (!hasRank[r] && r > boardTopRank) {
                outs++; // Pair outs
            }
        }
    }

    // Cap outs at 25 (reasonable maximum)
    return std::min(outs, 25);
}

double HandEvaluator::calculateEquity(const CardSet& holeCards, const CardSet& board, int iterations) {
    std::mt19937 rng(std::random_device{}());
    int wins = 0;
    int ties = 0;

    // Create deck excluding known cards
    std::array<Card, 52> deckArr;
    size_t deckSize = 0;
    for (Suit s : allSuits) {
        for (Rank r : allRanks) {
            Card c(r, s);
            bool used = false;
            for (size_t i = 0; i < holeCards.count; ++i) {
                if (holeCards.cards[i] == c) { used = true; break; }
            }
            for (size_t i = 0; i < board.count; ++i) {
                if (board.cards[i] == c) { used = true; break; }
            }
            if (!used) deckArr[deckSize++] = c;
        }
    }

    CardSet heroHand = holeCards;
    (void)heroHand; // May be used in future

    for (int iter = 0; iter < iterations; ++iter) {
        // Shuffle remaining deck
        std::shuffle(deckArr.begin(), deckArr.begin() + deckSize, rng);

        // Deal remaining board cards
        CardSet simBoard = board;
        size_t cardsNeeded = 5 - simBoard.count;
        for (size_t i = 0; i < cardsNeeded; ++i) {
            simBoard.add(deckArr[i]);
        }

        // Deal villain cards
        CardSet villainHand;
        villainHand.add(deckArr[cardsNeeded]);
        villainHand.add(deckArr[cardsNeeded + 1]);

        // Evaluate
        CardSet heroFull;
        for (size_t i = 0; i < heroHand.count; ++i) heroFull.add(heroHand.cards[i]);
        for (size_t i = 0; i < simBoard.count; ++i) heroFull.add(simBoard.cards[i]);

        CardSet villainFull;
        for (size_t i = 0; i < villainHand.count; ++i) villainFull.add(villainHand.cards[i]);
        for (size_t i = 0; i < simBoard.count; ++i) villainFull.add(simBoard.cards[i]);

        HandResult heroResult = evaluate(heroFull);
        HandResult villainResult = evaluate(villainFull);

        if (heroResult > villainResult) wins++;
        else if (heroResult == villainResult) ties++;
    }

    return (wins + 0.5 * ties) / static_cast<double>(iterations);
}

std::string HandEvaluator::cardRankToString(Rank rank) {
    switch (rank) {
        case Rank::Two:   return "2";
        case Rank::Three: return "3";
        case Rank::Four:  return "4";
        case Rank::Five:  return "5";
        case Rank::Six:   return "6";
        case Rank::Seven: return "7";
        case Rank::Eight: return "8";
        case Rank::Nine:  return "9";
        case Rank::Ten:   return "T";
        case Rank::Jack:  return "J";
        case Rank::Queen: return "Q";
        case Rank::King:  return "K";
        case Rank::Ace:   return "A";
        default:          return "?";
    }
}

std::string HandEvaluator::describeHand(const CardSet& holeCards, const CardSet& board) {
    if (holeCards.count < 2) return "Unknown";

    CardSet fullHand;
    for (size_t i = 0; i < holeCards.count; ++i) fullHand.add(holeCards.cards[i]);
    for (size_t i = 0; i < board.count; ++i) fullHand.add(board.cards[i]);

    HandResult result = evaluate(fullHand);

    // For made hands, give detailed description
    if (result.rank >= HandRank::OnePair && board.count >= 3) {
        // Find what we paired with
        Card c1 = holeCards.cards[0];
        Card c2 = holeCards.cards[1];

        // Get board ranks
        std::array<bool, 15> boardRank{};
        for (size_t i = 0; i < board.count; ++i) {
            boardRank[rankValue(board.cards[i].rank())] = true;
        }

        // Check if we have a pair using hole cards
        bool c1Paired = boardRank[rankValue(c1.rank())];
        bool c2Paired = boardRank[rankValue(c2.rank())];
        bool pocketPair = (c1.rank() == c2.rank());

        if (result.rank == HandRank::OnePair) {
            if (pocketPair) {
                return std::format("Pocket pair of {}{}s", cardRankToString(c1.rank()), cardRankToString(c1.rank()));
            }
            int topBoard = 0;
            for (int i = 14; i >= 2; --i) {
                if (boardRank[i]) { topBoard = i; break; }
            }

            if (c1Paired && !c2Paired) {
                // C1 paired with board
                int kicker = rankValue(c2.rank());
                if (rankValue(c1.rank()) == topBoard) {
                    if (kicker >= 10) return "Top pair, great kicker";
                    if (kicker >= 7) return "Top pair, good kicker";
                    return "Top pair, weak kicker";
                }
                return std::format("Middle pair with {} kicker", kicker >= 10 ? "good" : "weak");
            }
            if (c2Paired && !c1Paired) {
                int kicker = rankValue(c1.rank());
                if (rankValue(c2.rank()) == topBoard) {
                    if (kicker >= 10) return "Top pair, great kicker";
                    if (kicker >= 7) return "Top pair, good kicker";
                    return "Top pair, weak kicker";
                }
                return "Middle pair";
            }
            return "One pair";
        }

        if (result.rank == HandRank::TwoPair) {
            if (pocketPair) return "Two pair (overpair + board pair)";
            if (c1Paired && c2Paired) return "Two pair (both hole cards paired)";
            return "Two pair";
        }

        if (result.rank == HandRank::ThreeOfAKind) {
            if (pocketPair) return std::format("Set of {}{}s", cardRankToString(c1.rank()), cardRankToString(c1.rank()));
            return "Trips";
        }

        if (result.rank == HandRank::Straight) return "Straight";
        if (result.rank == HandRank::Flush) return "Flush";
        if (result.rank == HandRank::FullHouse) return "Full house";
        if (result.rank == HandRank::FourOfAKind) return "Quads";
        if (result.rank >= HandRank::StraightFlush) return "Straight flush";
    }

    // Draw descriptions
    if (board.count >= 3 && result.rank == HandRank::HighCard) {
        int outs = countOuts(holeCards, board);
        if (outs >= 10) return std::format("Strong draw ({}+ outs)", outs);
        if (outs >= 6) return std::format("Draw ({} outs)", outs);
        if (outs >= 3) return "Weak draw";
        return "High card";
    }

    return rankToString(result.rank);
}

} // namespace sharkwave
