#include "decision_engine.h"
#include "gto_charts.h"
#include <format>
#include <algorithm>
#include <cmath>
#include <array>

namespace sharkwave {

namespace {
    // Local helper functions (not in hand_evaluator namespace)
    constexpr int rankToInt(Rank r) { return static_cast<int>(r); }

    std::array<int, 15> countRanks(const Card* cards, size_t count) {
        std::array<int, 15> counts{};
        for (size_t i = 0; i < count; ++i) {
            counts[rankToInt(cards[i].rank())]++;
        }
        return counts;
    }

    std::array<int, 4> countSuits(const Card* cards, size_t count) {
        std::array<int, 4> counts{};
        for (size_t i = 0; i < count; ++i) {
            counts[static_cast<int>(cards[i].suit())]++;
        }
        return counts;
    }
}

DecisionEngine::DecisionEngine(GameSession& session)
    : session_(session)
{
}

Decision DecisionEngine::makeDecision() {
    switch (session_.street()) {
        case Street::Preflop:  return decidePreflop();
        case Street::Flop:     return decideFlop();
        case Street::Turn:     return decideTurn();
        case Street::River:    return decideRiver();
        default:               return Decision::fold("Unknown street");
    }
}

Decision DecisionEngine::decidePreflop() {
    (void)bigBlindsRemaining(); // Cache for potential use
    (void)getPosition(); // Cache for potential use

    // Unopened pot (we're first to act or everyone folded to us)
    // Simplified: assume unopened for now
    return decidePreflopUnopened();
}

Decision DecisionEngine::decidePreflopUnopened() {
    int bb = bigBlindsRemaining();
    Position pos = getPosition();
    HandCategory category = categorizeHoleCards();
    (void)category; // May be used later

    // Get GTO action from charts
    GtoDecision gto = GtoCharts::getAction(pos, session_.heroCards(), bb, false);

    switch (gto.action) {
        case GtoAction::Fold:
            return Decision::fold("Too weak to open from this position");

        case GtoAction::Call:
            // Opening can't be a call, but chart might return this for vs-raise
            return Decision::raise(getOpenRaiseSize(),
                "Best hand. Raising for value and isolation.");

        case GtoAction::Raise:
            return Decision::raise(getOpenRaiseSize(),
                "Raising for value and initiative");

        case GtoAction::AllIn:
            return Decision::raise(session_.heroStack(),
                "All-in for value with premium hand");

        default:
            return Decision::fold("Hand not in opening range");
    }
}

Decision DecisionEngine::decidePreflopVsRaise() {
    // This would be called when facing a raise before us
    // For MVP, simplified logic
    HandCategory category = categorizeHoleCards();
    int64_t raiseAmt = session_.toCall();

    switch (category) {
        case HandCategory::Premium:
            return Decision::raise(get3betSize(), "Premium hand. 3-bet for value");

        case HandCategory::Strong:
            if (bigBlindsRemaining() > 100) {
                return Decision::call(raiseAmt, "Strong hand. Call in position");
            }
            return Decision::raise(get3betSize(), "Strong hand. 3-bet or ship");

        case HandCategory::Medium:
            if (raiseAmt <= session_.pot() * 0.3) {
                return Decision::call(raiseAmt, "Medium hand. Good pot odds to call");
            }
            return Decision::fold("Medium hand. Fold to large raise");

        default:
            return Decision::fold("Weak hand. Fold to aggression");
    }
}

Decision DecisionEngine::decidePreflopVs3bet() {
    HandCategory category = categorizeHoleCards();

    switch (category) {
        case HandCategory::Premium:
        case HandCategory::Strong:
            if (bigBlindsRemaining() < 50) {
                return Decision::raise(session_.heroStack(), "All-in with strong hand");
            }
            return Decision::raise(get4betSize(), "4-bet for value");

        case HandCategory::Medium:
            if (session_.potOdds() < 0.3) {
                return Decision::call(session_.toCall(), "Call with decent pot odds");
            }
            return Decision::fold("Fold medium hand to 3-bet");

        default:
            return Decision::fold("Fold weak hand to 3-bet");
    }
}

Decision DecisionEngine::decidePreflopVs4bet() {
    HandCategory category = categorizeHoleCards();

    if (category == HandCategory::Premium) {
        return Decision::raise(session_.heroStack(), "All-in with premiums");
    }

    if (category == HandCategory::Strong && isPair(session_.heroCards().cards[0], session_.heroCards().cards[1])) {
        return Decision::raise(session_.heroStack(), "All-in with QQ+");
    }

    return Decision::fold("Fold to 4-bet without premiums");
}

Decision DecisionEngine::decideFlop() {
    // Combine hole cards and board for evaluation
    CardSet fullHand;
    for (size_t i = 0; i < session_.heroCards().count; ++i) {
        fullHand.add(session_.heroCards().cards[i]);
    }
    for (size_t i = 0; i < session_.board().count; ++i) {
        fullHand.add(session_.board().cards[i]);
    }

    HandResult hand = HandEvaluator::evaluate(fullHand);
    double equity = HandEvaluator::calculateEquity(session_.heroCards(), session_.board(), 500);
    double potOdds = session_.potOdds();

    // Check if we're facing a bet
    if (session_.toCall() > 0) {
        int64_t callAmt = session_.toCall();

        // We have a made hand
        if (hand.rank >= HandRank::TwoPair) {
            if (hand.rank >= HandRank::Straight || equity > 0.8) {
                return Decision::raise(callAmt * 2, "Strong hand. Raise for value");
            }
            return Decision::call(callAmt, "Good made hand. Call for value");
        }

        // We have a draw
        int outs = HandEvaluator::countOuts(session_.heroCards(), session_.board());
        if (outs >= 8) { // Strong draw
            if (equity > potOdds || equity > 0.35) {
                if (HandEvaluator::hasFlushDraw(session_.heroCards(), session_.board())) {
                    return Decision::call(callAmt, "Flush draw. Call with good odds");
                }
                return Decision::call(callAmt, std::format("Strong draw ({} outs). Call.", outs));
            }
        }

        // Weak hand - check fold equity
        if (equity < 0.25) {
            // Maybe bluff catch if pot odds good
            if (potOdds < 0.2) {
                return Decision::call(callAmt, "Bluff catch with good pot odds");
            }
            return Decision::fold("Weak hand. Fold to bet");
        }

        // Marginal hand
        return Decision::call(callAmt, "Marginal hand. Call to see turn");
    }

    // We're first to act or checked to
    BoardTexture texture = analyzeBoardTexture();

    // Value betting
    if (equity > 0.7 && hand.rank >= HandRank::OnePair) {
        int64_t betSize = getValueBetSize();
        return Decision::bet(betSize, "Value bet with strong hand");
    }

    // Semi-bluff with draws
    int outs = HandEvaluator::countOuts(session_.heroCards(), session_.board());
    if (outs >= 8 && texture == BoardTexture::Dry) {
        int64_t betSize = getCBetSize();
        return Decision::bet(betSize, std::format("Semi-bluff with {} outs. Good fold equity on dry board.", outs));
    }

    // Continuation bet
    if (equity > 0.5 || (isInPosition() && equity > 0.4)) {
        int64_t betSize = getCBetSize();
        return Decision::bet(betSize, "Continuation bet with equity advantage");
    }

    // Check with weak hand
    return Decision::check("Check with weak hand. Control pot size");
}

Decision DecisionEngine::decideTurn() {
    // Combine hole cards and board for evaluation
    CardSet fullHand;
    for (size_t i = 0; i < session_.heroCards().count; ++i) {
        fullHand.add(session_.heroCards().cards[i]);
    }
    for (size_t i = 0; i < session_.board().count; ++i) {
        fullHand.add(session_.board().cards[i]);
    }

    HandResult hand = HandEvaluator::evaluate(fullHand);
    double equity = HandEvaluator::calculateEquity(session_.heroCards(), session_.board(), 500);
    double potOdds = session_.potOdds();

    if (session_.toCall() > 0) {
        int64_t callAmt = session_.toCall();

        // Strong made hand
        if (hand.rank >= HandRank::ThreeOfAKind || equity > 0.8) {
            if (equity > 0.9) {
                return Decision::raise(callAmt * 2, "Monster. Raise for value");
            }
            return Decision::call(callAmt, "Strong hand. Call down");
        }

        // Two pair or better
        if (hand.rank >= HandRank::TwoPair) {
            if (potOdds < 0.35) {
                return Decision::call(callAmt, "Value call with two pair+");
            }
            return Decision::fold("Pot too large, fold to aggression");
        }

        // Draws
        int outs = HandEvaluator::countOuts(session_.heroCards(), session_.board());
        if (outs >= 6) {
            double needed = potOdds;
            double approxEquity = outs / 47.0;
            if (approxEquity > needed * 0.8) {
                return Decision::call(callAmt, std::format("Call with {} outs and good odds", outs));
            }
        }

        // Weak hands
        if (equity < 0.3) {
            if (potOdds < 0.15) {
                return Decision::call(callAmt, "Bluff catch in big pot");
            }
            return Decision::fold("Weak hand. Fold");
        }

        return Decision::call(callAmt, "Showdown value call");
    }

    // First to act or checked to
    if (equity > 0.75) {
        int64_t betSize = getValueBetSize();
        return Decision::bet(betSize, "Value bet with very strong hand");
    }

    if (equity > 0.6 && hand.rank >= HandRank::OnePair) {
        int64_t betSize = getValueBetSize();
        return Decision::bet(betSize, "Value bet with good hand");
    }

    // Bluff with backdoor or weak draws
    if (equity > 0.35 && session_.spr() > 3) {
        int64_t betSize = getBluffSize();
        return Decision::bet(betSize, "Probe bet with equity");
    }

    return Decision::check("Check with marginal hand");
}

Decision DecisionEngine::decideRiver() {
    // Combine hole cards and board for evaluation
    CardSet fullHand;
    for (size_t i = 0; i < session_.heroCards().count; ++i) {
        fullHand.add(session_.heroCards().cards[i]);
    }
    for (size_t i = 0; i < session_.board().count; ++i) {
        fullHand.add(session_.board().cards[i]);
    }

    HandResult hand = HandEvaluator::evaluate(fullHand);
    double potOdds = session_.potOdds();

    if (session_.toCall() > 0) {
        int64_t callAmt = session_.toCall();

        // Strong value hands
        if (hand.rank >= HandRank::Straight) {
            return Decision::call(callAmt, "Call with strong hand. Got there.");
        }

        if (hand.rank >= HandRank::ThreeOfAKind) {
            return Decision::call(callAmt, "Call with set+. Likely good.");
        }

        if (hand.rank >= HandRank::TwoPair) {
            // Need better pot odds for two pair
            if (potOdds < 0.4) {
                return Decision::call(callAmt, "Call with two pair. Good enough.");
            }
            return Decision::fold("Two pair but facing big bet. Fold.");
        }

        // One pair or worse - hero call or fold
        if (potOdds < 0.25) {
            return Decision::call(callAmt, "Bluff catch in massive pot");
        }

        return Decision::fold("Weak hand. Fold to bet");
    }

    // First to act or checked to - value bet or check
    if (hand.rank >= HandRank::ThreeOfAKind) {
        int64_t betSize = getValueBetSize();
        return Decision::bet(betSize, "Big value bet with monster");
    }

    if (hand.rank >= HandRank::TwoPair) {
        int64_t betSize = getValueBetSize() * 0.7;
        return Decision::bet(betSize, "Value bet with two pair+");
    }

    if (hand.rank >= HandRank::OnePair) {
        int64_t betSize = getValueBetSize() * 0.5;
        return Decision::bet(betSize, "Thin value with top pair");
    }

    // Maybe bluff missed draw?
    if (hand.rank <= HandRank::HighCard && session_.spr() > 2) {
        int64_t betSize = getBluffSize();
        return Decision::bet(betSize, "Bluff with missed draw. Represent something.");
    }

    return Decision::check("Check at showdown. Can't value bet weak hands");
}

double DecisionEngine::calculateEV(Action action, int64_t amount) {
    // Simplified EV calculation
    // Full version would consider ranges and frequencies
    double equity = getHandStrength();
    int64_t pot = session_.pot();

    switch (action) {
        case Action::Fold:
            return 0.0;

        case Action::Check:
            return pot * equity;

        case Action::Call: {
            double potOdds = session_.potOdds();
            if (equity > potOdds) {
                return (pot + amount) * equity - amount;
            }
            return -amount;
        }

        case Action::Bet:
        case Action::Raise: {
            double foldEquity = getFoldEquity();
            double evWhenCalled = (pot + amount * 2) * equity - amount;
            return foldEquity * pot + (1.0 - foldEquity) * evWhenCalled;
        }

        default:
            return 0.0;
    }
}

double DecisionEngine::getHandStrength() {
    return HandEvaluator::calculateEquity(session_.heroCards(), session_.board(), 500);
}

double DecisionEngine::getFoldEquity() {
    // Simplified - real implementation would use opponent models
    BoardTexture texture = analyzeBoardTexture();
    double baseFE = 0.3;

    if (texture == BoardTexture::Dry) baseFE += 0.2;
    if (texture == BoardTexture::VeryWet) baseFE -= 0.1;
    if (isInPosition()) baseFE += 0.1;

    return std::clamp(baseFE, 0.1, 0.6);
}

int64_t DecisionEngine::getOpenRaiseSize() {
    (void)bigBlindsRemaining(); // Cache for potential use
    Position pos = getPosition();

    int64_t bb = session_.bb();

    // Standard open sizes by position
    if (pos == Position::BTN || pos == Position::CO) {
        return std::min(static_cast<int64_t>(bb * 2.5), session_.heroStack());
    }
    if (pos == Position::SB) {
        return std::min(static_cast<int64_t>(bb * 2.0), session_.heroStack());
    }
    return std::min(static_cast<int64_t>(bb * 2.5), session_.heroStack());
}

int64_t DecisionEngine::get3betSize() {
    int64_t pot = session_.pot();
    int64_t size = static_cast<int64_t>(pot * 2.5);
    return std::min(size, session_.heroStack());
}

int64_t DecisionEngine::get4betSize() {
    int64_t pot = session_.pot() + session_.toCall() * 2;
    int64_t size = static_cast<int64_t>(pot * 2.2);
    return std::min(size, session_.heroStack());
}

int64_t DecisionEngine::getCBetSize() {
    int64_t pot = session_.pot();
    return static_cast<int64_t>(pot * 0.33);
}

int64_t DecisionEngine::getValueBetSize() {
    int64_t pot = session_.pot();
    double spr = session_.spr();

    if (spr < 2) return pot; // Small pot, shove
    if (spr < 4) return static_cast<int64_t>(pot * 0.75);
    return static_cast<int64_t>(pot * 0.5);
}

int64_t DecisionEngine::getBluffSize() {
    int64_t pot = session_.pot();
    return static_cast<int64_t>(pot * 0.5); // Smaller bluffs
}

// Helper methods

DecisionEngine::HandCategory DecisionEngine::categorizeHoleCards() {
    if (session_.heroCards().count < 2) return HandCategory::Weak;

    Card c1 = session_.heroCards().cards[0];
    Card c2 = session_.heroCards().cards[1];

    // Pairs
    if (isPair(c1, c2)) {
        if (c1.rank() >= Rank::Jack) return HandCategory::Premium; // JJ+
        if (c1.rank() >= Rank::Eight) return HandCategory::Medium;  // 88-TT
        return HandCategory::Speculative;                           // 22-77
    }

    // High card hands
    int hv = highCardValue(c1, c2);
    bool suited = isSuited(c1, c2);

    if (hv >= 30) { // AK, AQ
        return (hv >= 32) ? HandCategory::Premium : HandCategory::Strong;
    }

    if (hv >= 24) { // AJ, KQ
        return suited ? HandCategory::Medium : HandCategory::Medium;
    }

    // Suited connectors and one-gappers
    if (suited && isSuitedConnector(c1, c2)) {
        return HandCategory::Speculative;
    }

    return HandCategory::Weak;
}

bool DecisionEngine::isSuitedConnector(Card c1, Card c2) {
    if (c1.suit() != c2.suit()) return false;
    int diff = std::abs(rankToInt(c1.rank()) - rankToInt(c2.rank()));
    return diff <= 2;
}

bool DecisionEngine::isPair(Card c1, Card c2) {
    return c1.rank() == c2.rank();
}

bool DecisionEngine::isSuited(Card c1, Card c2) {
    return c1.suit() == c2.suit();
}

int DecisionEngine::highCardValue(Card c1, Card c2) {
    int v1 = rankToInt(c1.rank());
    int v2 = rankToInt(c2.rank());
    return std::max(v1, v2) * 2 + std::min(v1, v2) / 2;
}

DecisionEngine::BoardTexture DecisionEngine::analyzeBoardTexture() {
    if (session_.board().count < 3) return BoardTexture::Dry;

    bool flushPossible = hasFlushDrawOnBoard();
    bool straightPossible = hasStraightDrawOnBoard();
    bool paired = false;

    // Check for pairs on board
    auto counts = countRanks(session_.board().cards, session_.board().count);
    for (int c : counts) {
        if (c > 1) paired = true;
    }

    if (flushPossible && straightPossible) return BoardTexture::VeryWet;
    if (flushPossible || straightPossible || paired) return BoardTexture::Wet;
    return BoardTexture::Dry;
}

bool DecisionEngine::isDryBoard() {
    return analyzeBoardTexture() == BoardTexture::Dry;
}

bool DecisionEngine::hasFlushDrawOnBoard() {
    if (session_.board().count < 3) return false;

    auto suitCounts = countSuits(session_.board().cards, session_.board().count);
    for (int c : suitCounts) {
        if (c >= 2) return true; // 2 or more of same suit = flush draw possible
    }
    return false;
}

bool DecisionEngine::hasStraightDrawOnBoard() {
    if (session_.board().count < 3) return false;

    std::array<bool, 15> hasRank{};
    for (size_t i = 0; i < session_.board().count; ++i) {
        hasRank[rankToInt(session_.board().cards[i].rank())] = true;
    }

    // Check for 3 cards within 4-rank range
    for (int start = 10; start >= 1; --start) {
        int count = 0;
        for (int i = 0; i < 5; ++i) {
            if (hasRank[start + i]) count++;
        }
        if (count >= 3) return true;
    }

    return false;
}

bool DecisionEngine::isInPosition() {
    Position heroPos = getPosition();
    // Simplified: assume we're in position if we're BTN and villains are in blinds
    // Full version would track active players
    return heroPos == Position::BTN || heroPos == Position::CO;
}

bool DecisionEngine::isOutOfPosition() {
    return !isInPosition();
}

Position DecisionEngine::getPosition() {
    return session_.heroPosition();
}

bool DecisionEngine::isShortStack() {
    return bigBlindsRemaining() < 40;
}

bool DecisionEngine::isMediumStack() {
    int bb = bigBlindsRemaining();
    return bb >= 40 && bb <= 100;
}

bool DecisionEngine::isDeepStack() {
    return bigBlindsRemaining() > 100;
}

int DecisionEngine::bigBlindsRemaining() {
    return static_cast<int>(session_.heroStack() / session_.bb());
}

} // namespace sharkwave
