#pragma once

#include "card.h"
#include "game_session.h"
#include "decision_engine.h"
#include <random>

namespace sharkwave {

// Simple opponent AI for testing
enum class OpponentType {
    Random,      // Plays randomly
    TightPassive, // Only plays strong hands, rarely raises
    LooseAggressive, // Plays many hands, raises often
    CallingStation // Calls too much, rarely folds
};

class Simulation {
public:
    Simulation(int numHands = 1000, OpponentType oppType = OpponentType::Random);
    void run();
    void printResults();

private:
    struct SimHandResult {
        bool won;
        int64_t profit;
        bool reachedShowdown;
        bool heroWonShowdown;
    };

    SimHandResult playSingleHand();
    Card dealCard();
    void dealHoleCards();
    void dealFlop();
    void dealTurn();
    void dealRiver();
    void shuffleDeck();

    Action getOpponentAction(Position pos, const CardSet& holeCards, int64_t facingBet);
    Decision getHeroDecision();

    void settleShowdown();

    int numHands_;
    OpponentType opponentType_;

    // Deck state
    std::array<Card, 52> deck_;
    size_t deckIndex_;

    // Game state
    CardSet heroCards_;
    CardSet villainCards_;
    CardSet board_;

    int64_t heroStack_;
    int64_t villainStack_;
    int64_t pot_;

    int sb_, bb_;
    int heroHandsWon_;
    int villainHandsWon_;
    int64_t heroTotalProfit_;
    int showdownCount_;

    Position heroPosition_;
    Position villainPosition_;

    std::mt19937 rng_;

    static constexpr Position positions[] = {
        Position::UTG, Position::MP, Position::CO,
        Position::BTN, Position::SB, Position::BB
    };
};

} // namespace sharkwave
