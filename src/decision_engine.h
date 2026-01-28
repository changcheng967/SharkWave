#pragma once

#include "game_session.h"
#include "hand_evaluator.h"
#include <string>

namespace sharkwave {

struct Decision {
    Action action;
    int64_t amount;        // 0 for fold/check/call, bet/raise amount
    std::string reason;

    static Decision fold(std::string why) {
        return {Action::Fold, 0, std::move(why)};
    }

    static Decision check(std::string why) {
        return {Action::Check, 0, std::move(why)};
    }

    static Decision call(int64_t amt, std::string why) {
        return {Action::Call, amt, std::move(why)};
    }

    static Decision bet(int64_t amt, std::string why) {
        return {Action::Bet, amt, std::move(why)};
    }

    static Decision raise(int64_t amt, std::string why) {
        return {Action::Raise, amt, std::move(why)};
    }
};

class DecisionEngine {
public:
    DecisionEngine(GameSession& session);
    Decision makeDecision();

    // Preflop decisions
    Decision decidePreflop();
    Decision decidePreflopUnopened();
    Decision decidePreflopVsRaise();
    Decision decidePreflopVs3bet();
    Decision decidePreflopVs4bet();

    // Postflop decisions
    Decision decideFlop();
    Decision decideTurn();
    Decision decideRiver();

    // Calculation helpers
    double calculateEV(Action action, int64_t amount);
    double getHandStrength();
    double getFoldEquity();

    // Sizing helpers
    int64_t getOpenRaiseSize();
    int64_t get3betSize();
    int64_t get4betSize();
    int64_t getCBetSize();
    int64_t getValueBetSize();
    int64_t getBluffSize();

private:
    GameSession& session_;

    // Hand category (for preflop)
    enum class HandCategory {
        Premium,    // AA, KK
        Strong,     // QQ, AK
        Medium,     // JJ, AQ, KQ, AJ
        Speculative, // Small pairs, suited connectors
        Weak        // Everything else
    };

    HandCategory categorizeHoleCards();
    bool isSuitedConnector(Card c1, Card c2);
    bool isPair(Card c1, Card c2);
    bool isSuited(Card c1, Card c2);
    int highCardValue(Card c1, Card c2);

    // Board analysis
    enum class BoardTexture {
        Dry,        // No draws, disconnected
        Wet,        // Flush draw possible
        VeryWet,    // Multiple draws possible
        Coordinated // Highly connected
    };

    BoardTexture analyzeBoardTexture();
    bool isDryBoard();
    bool hasFlushDrawOnBoard();
    bool hasStraightDrawOnBoard();

    // Position helpers
    bool isInPosition();
    bool isOutOfPosition();
    Position getPosition();

    // Stack depth
    bool isShortStack();   // < 40bb
    bool isMediumStack();  // 40-100bb
    bool isDeepStack();    // > 100bb
    int bigBlindsRemaining();
};

} // namespace sharkwave
