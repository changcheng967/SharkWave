#include "simulation.h"
#include "hand_evaluator.h"
#include <iostream>
#include <format>

namespace sharkwave {

Simulation::Simulation(int numHands, OpponentType oppType)
    : numHands_(numHands)
    , opponentType_(oppType)
    , deckIndex_(0)
    , heroStack_(1000)
    , villainStack_(1000)
    , pot_(0)
    , sb_(5)
    , bb_(10)
    , heroHandsWon_(0)
    , villainHandsWon_(0)
    , heroTotalProfit_(0)
    , showdownCount_(0)
    , heroPosition_(Position::BTN)
    , villainPosition_(Position::BB)
    , rng_(std::random_device{}())
{
}

void Simulation::shuffleDeck() {
    deckIndex_ = 0;

    // Create deck
    size_t idx = 0;
    for (Suit s : allSuits) {
        for (Rank r : allRanks) {
            deck_[idx++] = Card(r, s);
        }
    }

    // Fisher-Yates shuffle
    for (int i = 51; i > 0; --i) {
        std::uniform_int_distribution<int> dist(0, i);
        int j = dist(rng_);
        std::swap(deck_[i], deck_[j]);
    }
}

Card Simulation::dealCard() {
    if (deckIndex_ >= 52) {
        shuffleDeck(); // Shouldn't happen in normal play
    }
    return deck_[deckIndex_++];
}

void Simulation::dealHoleCards() {
    heroCards_.clear();
    villainCards_.clear();

    // Deal hero first (position dependent)
    if (static_cast<int>(heroPosition_) < static_cast<int>(villainPosition_)) {
        heroCards_.add(dealCard());
        villainCards_.add(dealCard());
        heroCards_.add(dealCard());
        villainCards_.add(dealCard());
    } else {
        villainCards_.add(dealCard());
        heroCards_.add(dealCard());
        villainCards_.add(dealCard());
        heroCards_.add(dealCard());
    }
}

void Simulation::dealFlop() {
    board_.clear();
    dealCard(); // Burn card
    board_.add(dealCard());
    board_.add(dealCard());
    board_.add(dealCard());
}

void Simulation::dealTurn() {
    dealCard(); // Burn card
    board_.add(dealCard());
}

void Simulation::dealRiver() {
    dealCard(); // Burn card
    board_.add(dealCard());
}

Action Simulation::getOpponentAction(Position pos, const CardSet& holeCards, int64_t facingBet) {
    (void)pos; // Position affects decision but not used in simple implementation
    // Get hand strength for decision making
    CardSet combined;
    for (size_t i = 0; i < holeCards.count; ++i) combined.add(holeCards.cards[i]);
    for (size_t i = 0; i < board_.count; ++i) combined.add(board_.cards[i]);
    HandResult hand = HandEvaluator::evaluate(combined);

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double rand = dist(rng_);

    switch (opponentType_) {
        case OpponentType::Random:
            if (facingBet > 0) {
                // 50% fold, 30% call, 20% raise to bet
                if (rand < 0.5) return Action::Fold;
                if (rand < 0.8) return Action::Call;
                return Action::Raise;
            }
            // 30% check, 50% bet, 20% raise
            if (rand < 0.3) return Action::Check;
            if (rand < 0.8) return Action::Bet;
            return Action::Raise;

        case OpponentType::TightPassive:
            if (facingBet > 0) {
                // Only calls with strong hands
                if (hand.rank >= HandRank::TwoPair) return Action::Call;
                if (hand.rank >= HandRank::OnePair && rand < 0.3) return Action::Call;
                return Action::Fold;
            }
            // Only bets strong hands
            if (hand.rank >= HandRank::ThreeOfAKind) return Action::Bet;
            if (hand.rank >= HandRank::TwoPair && rand < 0.5) return Action::Bet;
            return Action::Check;

        case OpponentType::LooseAggressive:
            if (facingBet > 0) {
                // Calls wide, raises often
                if (rand < 0.2) return Action::Fold;
                if (rand < 0.6) return Action::Call;
                return Action::Raise;
            }
            // Bets often
            if (rand < 0.2) return Action::Check;
            if (rand < 0.7) return Action::Bet;
            return Action::Raise;

        case OpponentType::CallingStation:
            if (facingBet > 0) {
                // Almost never folds
                if (rand < 0.95) return Action::Call;
                return Action::Fold;
            }
            // Passive, rarely bets
            if (rand < 0.8) return Action::Check;
            return Action::Bet;
    }

    return Action::Check;
}

Decision Simulation::getHeroDecision() {
    // Create a mock GameSession for the DecisionEngine
    // This is a simplified version - full implementation would integrate better
    CardSet fullHand;
    for (size_t i = 0; i < heroCards_.count; ++i) fullHand.add(heroCards_.cards[i]);
    for (size_t i = 0; i < board_.count; ++i) fullHand.add(board_.cards[i]);

    HandResult hand = HandEvaluator::evaluate(fullHand);
    double equity = HandEvaluator::calculateEquity(heroCards_, board_, 500);

    // Simple GTO-ish decisions
    bool facingBet = (pot_ > 0); // Simplified

    if (board_.count == 0) {
        // Preflop - use GTO charts
        return Decision::raise(25, "Opening raise with playable hand");
    }

    if (facingBet) {
        int64_t callAmt = pot_ / 2; // Simplified

        if (hand.rank >= HandRank::TwoPair || equity > 0.7) {
            return Decision::raise(callAmt * 2, "Strong hand. Raise for value");
        }
        if (hand.rank >= HandRank::OnePair || equity > 0.4) {
            return Decision::call(callAmt, "Decent hand. Call to see next street");
        }
        return Decision::fold("Weak hand. Fold to aggression");
    }

    // First to act
    if (equity > 0.6) {
        return Decision::bet(static_cast<int64_t>(pot_ * 0.5), "Value bet with strong hand");
    }
    if (equity > 0.45) {
        int64_t outs = HandEvaluator::countOuts(heroCards_, board_);
        if (outs >= 6) {
            return Decision::bet(static_cast<int64_t>(pot_ * 0.33), "Semi-bluff with draw");
        }
    }
    return Decision::check("Check with marginal hand");
}

void Simulation::settleShowdown() {
    CardSet heroFull, villainFull;

    for (size_t i = 0; i < heroCards_.count; ++i) heroFull.add(heroCards_.cards[i]);
    for (size_t i = 0; i < villainCards_.count; ++i) villainFull.add(villainCards_.cards[i]);
    for (size_t i = 0; i < board_.count; ++i) {
        heroFull.add(board_.cards[i]);
        villainFull.add(board_.cards[i]);
    }

    HandResult heroHand = HandEvaluator::evaluate(heroFull);
    HandResult villainHand = HandEvaluator::evaluate(villainFull);

    bool heroWins = (heroHand > villainHand);
    bool tie = (heroHand == villainHand);

    if (heroWins) {
        heroStack_ += pot_;
        heroHandsWon_++;
    } else if (tie) {
        heroStack_ += pot_ / 2;
        villainStack_ += pot_ / 2;
    } else {
        villainStack_ += pot_;
        villainHandsWon_++;
    }

    pot_ = 0;
    showdownCount_++;
}

Simulation::SimHandResult Simulation::playSingleHand() {
    shuffleDeck();

    // Reset stacks for this hand (simplified - real simulation would track across hands)
    heroStack_ = 1000;
    villainStack_ = 1000;
    pot_ = 0;

    // Post blinds (simplified - always hero on BTN for HU)
    heroPosition_ = Position::BTN;
    villainPosition_ = Position::BB;

    // Hero is BTN/SB, Villain is BB
    int64_t heroBlind = sb_;
    int64_t villainBlind = bb_;

    heroStack_ -= heroBlind;
    villainStack_ -= villainBlind;
    pot_ = heroBlind + villainBlind;

    // Deal hole cards
    dealHoleCards();

    bool handOver = false;
    bool heroFolded = false;
    bool villainFolded = false;
    bool reachedShowdown = false;

    // === PREFLOP ===
    // Hero acts first in SB
    Decision heroDecision = getHeroDecision();

    if (heroDecision.action == Action::Fold) {
        heroFolded = true;
        handOver = true;
        villainStack_ += pot_;
        pot_ = 0;
    } else if (heroDecision.action == Action::Raise) {
        int64_t raiseAmt = heroDecision.amount;
        heroStack_ -= raiseAmt;
        pot_ += raiseAmt;

        // Villain responds
        Action villainAction = getOpponentAction(villainPosition_, villainCards_, raiseAmt);
        if (villainAction == Action::Fold) {
            villainFolded = true;
            handOver = true;
            heroStack_ += pot_;
            pot_ = 0;
        } else if (villainAction == Action::Call) {
            villainStack_ -= raiseAmt;
            pot_ += raiseAmt;
        } else if (villainAction == Action::Raise) {
            // Villain 3-bets - hero calls for simplicity
            int64_t threebet = raiseAmt * 2;
            villainStack_ -= threebet;
            pot_ += threebet;
            heroStack_ -= threebet;
            pot_ += threebet;
        }
    }

    // === FLOP ===
    if (!handOver) {
        dealFlop();

        // Villain acts first OOP
        Action villainAction = getOpponentAction(villainPosition_, villainCards_, 0);
        if (villainAction == Action::Bet) {
            int64_t betAmt = static_cast<int64_t>(pot_ * 0.5);
            villainStack_ -= betAmt;
            pot_ += betAmt;

            heroDecision = getHeroDecision();
            if (heroDecision.action == Action::Fold) {
                heroFolded = true;
                handOver = true;
                villainStack_ += pot_;
                pot_ = 0;
            } else if (heroDecision.action == Action::Call) {
                heroStack_ -= betAmt;
                pot_ += betAmt;
            } else if (heroDecision.action == Action::Raise) {
                int64_t raiseAmt = betAmt * 2;
                heroStack_ -= raiseAmt;
                pot_ += raiseAmt;
                // Villain folds to raise
                villainFolded = true;
                handOver = true;
                heroStack_ += pot_;
                pot_ = 0;
            }
        } else {
            // Villain checks
            heroDecision = getHeroDecision();
            if (heroDecision.action == Action::Bet) {
                int64_t betAmt = heroDecision.amount;
                heroStack_ -= betAmt;
                pot_ += betAmt;

                // Villain responds
                Action vResponse = getOpponentAction(villainPosition_, villainCards_, betAmt);
                if (vResponse == Action::Fold) {
                    villainFolded = true;
                    handOver = true;
                    heroStack_ += pot_;
                    pot_ = 0;
                } else if (vResponse == Action::Call) {
                    villainStack_ -= betAmt;
                    pot_ += betAmt;
                }
            }
        }
    }

    // === TURN ===
    if (!handOver) {
        dealTurn();

        Action villainAction = getOpponentAction(villainPosition_, villainCards_, 0);
        if (villainAction == Action::Bet) {
            int64_t betAmt = static_cast<int64_t>(pot_ * 0.5);
            villainStack_ -= betAmt;
            pot_ += betAmt;

            heroDecision = getHeroDecision();
            if (heroDecision.action == Action::Fold) {
                heroFolded = true;
                handOver = true;
                villainStack_ += pot_;
                pot_ = 0;
            } else if (heroDecision.action == Action::Call) {
                heroStack_ -= betAmt;
                pot_ += betAmt;
            }
        } else {
            heroDecision = getHeroDecision();
            if (heroDecision.action == Action::Bet) {
                int64_t betAmt = heroDecision.amount;
                heroStack_ -= betAmt;
                pot_ += betAmt;

                Action vResponse = getOpponentAction(villainPosition_, villainCards_, betAmt);
                if (vResponse == Action::Fold) {
                    villainFolded = true;
                    handOver = true;
                    heroStack_ += pot_;
                    pot_ = 0;
                } else if (vResponse == Action::Call) {
                    villainStack_ -= betAmt;
                    pot_ += betAmt;
                }
            }
        }
    }

    // === RIVER ===
    if (!handOver) {
        dealRiver();

        Action villainAction = getOpponentAction(villainPosition_, villainCards_, 0);
        if (villainAction == Action::Bet) {
            int64_t betAmt = static_cast<int64_t>(pot_ * 0.5);
            villainStack_ -= betAmt;
            pot_ += betAmt;

            heroDecision = getHeroDecision();
            if (heroDecision.action == Action::Fold) {
                heroFolded = true;
                handOver = true;
                villainStack_ += pot_;
                pot_ = 0;
            } else if (heroDecision.action == Action::Call) {
                heroStack_ -= betAmt;
                pot_ += betAmt;
            }
        } else {
            heroDecision = getHeroDecision();
            if (heroDecision.action == Action::Bet) {
                int64_t betAmt = heroDecision.amount;
                heroStack_ -= betAmt;
                pot_ += betAmt;

                Action vResponse = getOpponentAction(villainPosition_, villainCards_, betAmt);
                if (vResponse == Action::Fold) {
                    villainFolded = true;
                    handOver = true;
                    heroStack_ += pot_;
                    pot_ = 0;
                } else if (vResponse == Action::Call) {
                    villainStack_ -= betAmt;
                    pot_ += betAmt;
                }
            }
        }
    }

    // === SHOWDOWN ===
    if (!handOver) {
        reachedShowdown = true;
        settleShowdown();
    }

    int64_t profit = heroStack_ - 1000;
    return SimHandResult{!heroFolded && !villainFolded && profit >= 0, profit, reachedShowdown, !heroFolded};
}

void Simulation::run() {
    std::cout << "\n=== SHARKWAVE SIMULATION ===\n";
    std::cout << "Running " << numHands_ << " hands vs ";

    switch (opponentType_) {
        case OpponentType::Random: std::cout << "Random"; break;
        case OpponentType::TightPassive: std::cout << "Tight Passive"; break;
        case OpponentType::LooseAggressive: std::cout << "Loose Aggressive"; break;
        case OpponentType::CallingStation: std::cout << "Calling Station"; break;
    }
    std::cout << " opponent...\n\n";

    int64_t totalProfit = 0;
    int wins = 0;
    int losses = 0;
    int showdowns = 0;

    for (int i = 0; i < numHands_; ++i) {
        SimHandResult result = playSingleHand();
        totalProfit += result.profit;

        if (result.reachedShowdown) {
            showdowns++;
            if (result.heroWonShowdown) wins++;
            else losses++;
        } else if (result.won) {
            wins++;
        }
        // folded wins are counted in wins already

        // Progress indicator
        if ((i + 1) % 100 == 0) {
            std::cout << "  " << (i + 1) << " hands completed...\r" << std::flush;
        }
    }

    heroHandsWon_ = wins;
    villainHandsWon_ = losses;
    heroTotalProfit_ = totalProfit;
    showdownCount_ = showdowns;

    std::cout << "\n\nSimulation complete!\n";
}

void Simulation::printResults() {
    std::cout << "\n=== RESULTS ===\n";
    std::cout << "Hands played:     " << numHands_ << "\n";
    std::cout << "Hands won:        " << heroHandsWon_ << " (" << (100.0 * heroHandsWon_ / numHands_) << "%)\n";
    std::cout << "Hands lost:       " << villainHandsWon_ << " (" << (100.0 * villainHandsWon_ / numHands_) << "%)\n";
    std::cout << "Folds won:        " << (numHands_ - heroHandsWon_ - villainHandsWon_) << "\n";
    std::cout << "Showdowns:        " << showdownCount_ << "\n";
    std::cout << "\n";
    std::cout << "Total profit:     " << heroTotalProfit_ << " chips\n";
    std::cout << "Profit/100 hands: " << (100.0 * heroTotalProfit_ / numHands_) << " chips\n";
    std::cout << "Win rate:         " << (100.0 * heroHandsWon_ / numHands_) << "%\n";
    std::cout << "BB/100:           " << (heroTotalProfit_ / (double)numHands_ / bb_ * 100) << "\n";
    std::cout << "\n";

    if (heroTotalProfit_ > 0) {
        std::cout << ">>> SHARKWAVE IS WINNING <<<\n";
    } else if (heroTotalProfit_ < 0) {
        std::cout << ">>> SHARKWAVE IS LOSING <<<\n";
    } else {
        std::cout << ">>> BREAK EVEN <<<\n";
    }
    std::cout << "\n";
}

} // namespace sharkwave
