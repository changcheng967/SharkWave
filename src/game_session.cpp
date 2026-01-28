#include "game_session.h"
#include <format>
#include <algorithm>

namespace sharkwave {

GameSession::GameSession()
    : playerCount_(6)
    , sb_(5)
    , bb_(10)
    , heroPosition_(Position::CO)
    , heroStack_(1000)
    , opponentStacks_{}
    , pot_(0)
    , currentBet_(0)
    , toCall_(0)
    , street_(Street::Preflop)
    , actionCount_(0)
    , initialHeroStack_(1000)
    , sessionProfit_(0)
    , handsPlayed_(0)
    , handsWon_(0)
    , wonHand_(false)
{
    opponentStacks_.fill(1000);
}

void GameSession::reset() {
    playerCount_ = 6;
    sb_ = 5;
    bb_ = 10;
    heroStack_ = 1000;
    heroPosition_ = Position::CO;
    opponentStacks_.fill(1000);
    sessionProfit_ = 0;
    handsPlayed_ = 0;
    handsWon_ = 0;
    newHand();
}

void GameSession::newHand() {
    heroCards_.clear();
    board_.clear();
    pot_ = sb_ + bb_; // Blinds posted
    currentBet_ = bb_;
    toCall_ = 0;
    street_ = Street::Preflop;
    actionCount_ = 0;
    initialHeroStack_ = heroStack_;
    wonHand_ = false;

    // Post blinds based on position
    if (heroPosition_ == Position::SB) {
        heroStack_ -= sb_;
        pot_ += sb_;
        toCall_ = bb_ - sb_;
    } else if (heroPosition_ == Position::BB) {
        heroStack_ -= bb_;
        pot_ += bb_;
    }
}

void GameSession::setHeroCards(Card c1, Card c2) {
    heroCards_.clear();
    heroCards_.add(c1);
    heroCards_.add(c2);
}

void GameSession::setOpponentStack(Position pos, int64_t stack) {
    opponentStacks_[static_cast<int>(pos)] = stack;
}

void GameSession::setFlop(Card c1, Card c2, Card c3) {
    board_.clear();
    board_.add(c1);
    board_.add(c2);
    board_.add(c3);
    street_ = Street::Flop;
    currentBet_ = 0;
    toCall_ = 0;
}

void GameSession::setTurn(Card c) {
    board_.add(c);
    street_ = Street::Turn;
    currentBet_ = 0;
    toCall_ = 0;
}

void GameSession::setRiver(Card c) {
    board_.add(c);
    street_ = Street::River;
    currentBet_ = 0;
    toCall_ = 0;
}

void GameSession::recordAction(Position pos, Action action, int64_t amount) {
    if (actionCount_ < actionHistory_.size()) {
        actionHistory_[actionCount_++] = {pos, action, amount};
    }
}

void GameSession::processHeroAction(Action action, int64_t amount) {
    recordAction(heroPosition_, action, amount);

    switch (action) {
        case Action::Fold:
            toCall_ = 0;
            break;

        case Action::Check:
            // Nothing changes
            break;

        case Action::Call:
            heroStack_ -= toCall_;
            pot_ += toCall_;
            toCall_ = 0;
            break;

        case Action::Bet:
            heroStack_ -= amount;
            pot_ += amount;
            currentBet_ = amount;
            toCall_ = amount;
            break;

        case Action::Raise:
            int64_t totalAmount = toCall_ + amount;
            heroStack_ -= totalAmount;
            pot_ += totalAmount;
            currentBet_ += amount;
            toCall_ = currentBet_;
            break;
    }
}

void GameSession::advanceTo(Street street) {
    street_ = street;
    currentBet_ = 0;
    toCall_ = 0;
}

void GameSession::nextStreet() {
    switch (street_) {
        case Street::Preflop:
            street_ = Street::Flop;
            break;
        case Street::Flop:
            street_ = Street::Turn;
            break;
        case Street::Turn:
            street_ = Street::River;
            break;
        case Street::River:
            street_ = Street::Showdown;
            break;
        default:
            break;
    }
    currentBet_ = 0;
    toCall_ = 0;
}

int64_t GameSession::handProfit() const {
    return heroStack_ - initialHeroStack_ + (wonHand_ ? pot_ : 0);
}

double GameSession::potOdds() const {
    if (toCall_ == 0) return 0.0;
    return static_cast<double>(toCall_) / static_cast<double>(pot_ + toCall_);
}

double GameSession::spr() const {
    if (pot_ == 0) return 0.0;
    return static_cast<double>(effectiveStack()) / static_cast<double>(pot_);
}

int64_t GameSession::effectiveStack() const {
    int64_t minStack = heroStack_;
    for (int64_t stack : opponentStacks_) {
        if (stack > 0 && stack < minStack) {
            minStack = stack;
        }
    }
    return minStack;
}

std::string GameSession::positionToString(Position pos) {
    switch (pos) {
        case Position::UTG: return "UTG";
        case Position::MP:  return "MP";
        case Position::CO:  return "CO";
        case Position::BTN: return "BTN";
        case Position::SB:  return "SB";
        case Position::BB:  return "BB";
    }
    return "??";
}

std::string GameSession::actionToString(Action action) {
    switch (action) {
        case Action::Fold:  return "folds";
        case Action::Check: return "checks";
        case Action::Call:  return "calls";
        case Action::Bet:   return "bets";
        case Action::Raise: return "raises";
    }
    return "??";
}

std::string GameSession::streetToString(Street street) {
    switch (street) {
        case Street::Preflop:  return "PREFLOP";
        case Street::Flop:     return "FLOP";
        case Street::Turn:     return "TURN";
        case Street::River:    return "RIVER";
        case Street::Showdown: return "SHOWDOWN";
    }
    return "???";
}

} // namespace sharkwave
