#pragma once

#include "card.h"
#include <cstdint>
#include <string>
#include <array>

namespace sharkwave {

enum class Position : uint8_t {
    UTG,        // Under the Gun
    MP,         // Middle Position
    CO,         // Cutoff
    BTN,        // Button
    SB,         // Small Blind
    BB          // Big Blind
};

enum class Street : uint8_t {
    Preflop,
    Flop,
    Turn,
    River,
    Showdown
};

enum class Action : uint8_t {
    Fold,
    Check,
    Call,
    Bet,
    Raise
};

struct ActionRecord {
    Position position;
    Action action;
    int64_t amount;     // 0 for fold/check, positive for call/bet/raise
};

class GameSession {
public:
    GameSession();
    void reset();
    void newHand();

    // Game setup
    void setPlayerCount(int count) { playerCount_ = count; }
    void setBlinds(int small, int big) { sb_ = small; bb_ = big; }
    void setHeroStack(int64_t stack) { heroStack_ = stack; }
    void setHeroPosition(Position pos) { heroPosition_ = pos; }
    void setHeroCards(Card c1, Card c2);
    void setOpponentStack(Position pos, int64_t stack);

    // Board
    void setFlop(Card c1, Card c2, Card c3);
    void setTurn(Card c);
    void setRiver(Card c);

    // Game state
    Street street() const { return street_; }
    int64_t pot() const { return pot_; }
    int64_t heroStack() const { return heroStack_; }
    int64_t currentBet() const { return currentBet_; }
    int64_t toCall() const { return toCall_; }
    int sb() const { return sb_; }
    int bb() const { return bb_; }
    Position heroPosition() const { return heroPosition_; }

    // Actions
    void recordAction(Position pos, Action action, int64_t amount = 0);
    void processHeroAction(Action action, int64_t amount = 0);

    // Cards
    CardSet heroCards() const { return heroCards_; }
    CardSet board() const { return board_; }

    // Street progression
    void advanceTo(Street street);
    void nextStreet();

    // Session stats
    int64_t sessionProfit() const { return sessionProfit_; }
    int64_t handProfit() const;
    int handsPlayed() const { return handsPlayed_; }
    int handsWon() const { return handsWon_; }

    // Calculation helpers
    double potOdds() const;     // Returns as percentage (0.0 to 1.0)
    double spr() const;         // Stack-to-Pot Ratio
    int64_t effectiveStack() const;

    // String helpers
    static std::string positionToString(Position pos);
    static std::string actionToString(Action action);
    static std::string streetToString(Street street);

private:
    void updatePot(int64_t amount) { pot_ += amount; }
    void deductFromStack(int64_t amount) { heroStack_ -= amount; }

    // Game state
    int playerCount_;
    int sb_, bb_;
    Position heroPosition_;
    int64_t heroStack_;
    std::array<int64_t, 6> opponentStacks_;  // indexed by Position

    // Cards
    CardSet heroCards_;
    CardSet board_;

    // Betting
    int64_t pot_;
    int64_t currentBet_;
    int64_t toCall_;
    Street street_;

    // Hand history
    std::array<ActionRecord, 50> actionHistory_;
    size_t actionCount_;
    int64_t initialHeroStack_;

    // Session tracking
    int64_t sessionProfit_;
    int handsPlayed_;
    int handsWon_;
    bool wonHand_;
};

} // namespace sharkwave
