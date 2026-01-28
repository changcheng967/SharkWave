#include "simulation.h"
#include "hand_evaluator.h"
#include "gto_charts.h"
#include <iostream>
#include <format>

namespace sharkwave {

namespace {
    constexpr int rankToInt(Rank r) { return static_cast<int>(r); }
}

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

    // Heads up: BTN (hero) gets first card, BB (villain) gets second, then BTN first, BB second
    heroCards_.add(dealCard());
    villainCards_.add(dealCard());
    heroCards_.add(dealCard());
    villainCards_.add(dealCard());
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

Action Simulation::getOpponentAction(Position pos, const CardSet& holeCards, int64_t facingBet, bool canCheck) {
    (void)pos; // Position affects decision but not used in simple implementation
    // Get hand strength for decision making
    CardSet combined;
    for (size_t i = 0; i < holeCards.count; ++i) combined.add(holeCards.cards[i]);
    for (size_t i = 0; i < board_.count; ++i) combined.add(board_.cards[i]);
    ::sharkwave::HandResult hand = HandEvaluator::evaluate(combined);

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double rand = dist(rng_);

    // Pot odds calculation
    double potOdds = (facingBet > 0) ? static_cast<double>(facingBet) / (pot_ + facingBet * 2) : 0.0;

    // Get equity approximation
    double equity = HandEvaluator::calculateEquity(holeCards, board_, 200);

    switch (opponentType_) {
        case OpponentType::Random: {
            if (facingBet > 0) {
                if (rand < 0.35) return Action::Fold;
                if (rand < 0.75) return Action::Call;
                return Action::Raise;
            }
            // First to act
            if (rand < 0.40) return Action::Check;
            if (rand < 0.80) return Action::Bet;
            return Action::Raise;
        }

        case OpponentType::TightPassive: {
            if (facingBet > 0) {
                // Only calls with good pot odds or strong hands
                if (equity > potOdds + 0.1) return Action::Call;
                if (hand.rank >= ::sharkwave::HandRank::TwoPair) return Action::Call;
                if (hand.rank >= ::sharkwave::HandRank::OnePair && equity > 0.55) return Action::Call;
                return Action::Fold;
            }
            // Very passive, rarely bets
            if (rand < 0.85) return Action::Check;
            if (hand.rank >= ::sharkwave::HandRank::TwoPair) return Action::Bet;
            if (equity > 0.7 && rand < 0.5) return Action::Bet;
            return Action::Check;
        }

        case OpponentType::LooseAggressive: {
            if (facingBet > 0) {
                // Calls wide, raises often
                if (equity > 0.25) {
                    if (equity > 0.55 && rand < 0.4) return Action::Raise;
                    return Action::Call;
                }
                if (rand < 0.15) return Action::Raise; // Bluff raise
                if (rand < 0.40) return Action::Call;
                return Action::Fold;
            }
            // Aggressive betting
            if (rand < 0.15) return Action::Check;
            if (equity > 0.45 || rand < 0.30) return Action::Bet;
            if (rand < 0.50) return Action::Raise;
            return Action::Bet;
        }

        case OpponentType::CallingStation: {
            if (facingBet > 0) {
                // Almost never folds
                if (rand < 0.95) return Action::Call;
                return Action::Fold;
            }
            // Passive, rarely bets
            if (rand < 0.90) return Action::Check;
            return Action::Bet;
        }
    }

    return canCheck ? Action::Check : Action::Fold;
}

Decision Simulation::getHeroDecision(bool facingBet, int64_t facingAmt) {
    // Get hand strength
    CardSet fullHand;
    for (size_t i = 0; i < heroCards_.count; ++i) fullHand.add(heroCards_.cards[i]);
    for (size_t i = 0; i < board_.count; ++i) fullHand.add(board_.cards[i]);

    ::sharkwave::HandResult hand = HandEvaluator::evaluate(fullHand);
    double equity = HandEvaluator::calculateEquity(heroCards_, board_, 300);

    double potOdds = (facingAmt > 0) ? static_cast<double>(facingAmt) / (pot_ + facingAmt) : 0.0;

    // Preflop - HEADS UP GTO (very different from full ring!)
    if (board_.count == 0) {
        if (facingBet) {
            // Facing a 3-bet as SB opener
            // 4-bet premiums, call rest
            int r1 = rankToInt(heroCards_.cards[0].rank());
            int r2 = rankToInt(heroCards_.cards[1].rank());
            bool paired = (r1 == r2);
            (void)paired; // Used in condition
            int high = std::max(r1, r2);

            // 4-bet: AA, KK, AK
            if ((paired && r1 >= 13) || (high == 14 && r2 == 13)) {
                int64_t raiseAmt = heroStack_; // All-in
                return Decision::raise(raiseAmt, "4-bet all-in with premium");
            }

            // Call with most hands that raised
            if (equity > 0.45) {
                return Decision::call(facingAmt, "Call 3-bet with decent hand");
            }
            return Decision::fold("Fold to 3-bet with trash");
        }

        // Unopened pot in HU - RAISE ALMOST EVERYTHING from SB/BTN
        // Heads up: SB (button) should raise ~70-80% of hands
        int r1 = rankToInt(heroCards_.cards[0].rank());
        int r2 = rankToInt(heroCards_.cards[1].rank());
        bool paired = (r1 == r2);
        bool suited = (heroCards_.cards[0].suit() == heroCards_.cards[1].suit());
        int high = std::max(r1, r2);
        int low = std::min(r1, r2);

        // In heads up, open almost everything:
        // - All pairs
        // - All suited hands (A2s+, K2s+, Q4s+, J6s+, T7s+, 98s+)
        // - All offsuit A (A2o+)
        // - High offsuit connectors (KTo+, QTo+, JTo)
        // - Some Kx offsuit (K6o+)

        bool shouldOpen = paired || suited ||
            (high == 14) || // All Ax
            (high >= 12 && low >= 10) || // Broadway offsuit
            (high == 11 && low >= 10) || // JTo
            (high == 13 && low >= 7); // K7o+

        if (shouldOpen) {
            int64_t raiseAmt = static_cast<int64_t>(bb_ * 2.5);
            if (raiseAmt > heroStack_) raiseAmt = heroStack_;
            return Decision::raise(raiseAmt, "Heads up min-raise button");
        }

        return Decision::check("Check with trash in HU");
    }

    // Postflop decisions - EXPLOITATIVE based on opponent type
    if (facingBet) {
        // Adjust play based on opponent
        bool vsCallingStation = (opponentType_ == OpponentType::CallingStation);
        bool vsTightPassive = (opponentType_ == OpponentType::TightPassive);
        bool vsLAG = (opponentType_ == OpponentType::LooseAggressive);
        (void)vsTightPassive; // Used in threshold calculation

        // EXPLOIT: Value bet thinner vs calling stations
        double valueThreshold = vsCallingStation ? 0.50 : 0.65;
        double bluffCatchThreshold = vsCallingStation ? 0.35 : 0.25;

        if (hand.rank >= ::sharkwave::HandRank::TwoPair || equity > valueThreshold) {
            if (equity > 0.75) {
                int64_t raiseAmt = facingAmt * 2 + pot_;
                if (raiseAmt > heroStack_) raiseAmt = heroStack_;
                return Decision::raise(raiseAmt, "Raise for value with strong hand");
            }
            // EXPLOIT: Call more vs calling stations
            if (vsCallingStation && equity > 0.55) {
                return Decision::call(facingAmt, "Call for value vs station");
            }
            return Decision::call(facingAmt, "Call with good made hand");
        }

        if (hand.rank >= ::sharkwave::HandRank::OnePair || equity > 0.45) {
            if (potOdds < 0.40) {
                return Decision::call(facingAmt, "Call with pair or decent equity");
            }
            // EXPLOIT: Call lighter vs LAG but not too light
            if (vsLAG && equity > 0.32) {
                return Decision::call(facingAmt, "Call vs LAG with showdown value");
            }
            if (equity > 0.35) {
                return Decision::call(facingAmt, "Call with showdown value");
            }
            return Decision::fold("Fold to large bet without odds");
        }

        // Draw evaluation - semi-bluff raises with strong draws
        int outs = HandEvaluator::countOuts(heroCards_, board_);
        if (outs >= 12) {
            // Monster draw - raise semi-bluff
            int64_t raiseAmt = facingAmt + pot_;
            if (raiseAmt > heroStack_) raiseAmt = heroStack_;
            return Decision::raise(raiseAmt, std::format("Semi-bluff raise with {} outs", outs));
        }
        if (outs >= 8) {
            double approxEquity = outs / 47.0;
            if (approxEquity > potOdds * 0.7 || equity > 0.35) {
                return Decision::call(facingAmt, std::format("Call with {} outs and good odds", outs));
            }
        }

        // Weak hands - EXPLOIT: bluff catch selectively
        if (equity < bluffCatchThreshold) {
            double catchThreshold = vsLAG ? 0.30 : 0.25;
            if (potOdds < catchThreshold) {
                return Decision::call(facingAmt, "Bluff catch with good price");
            }
            return Decision::fold("Weak hand. Fold to bet");
        }

        return Decision::call(facingAmt, "Showdown value call");
    }

    // First to act or checked to - EXPLOITATIVE betting
    bool vsLAG = (opponentType_ == OpponentType::LooseAggressive);
    bool vsCallingStation = (opponentType_ == OpponentType::CallingStation);
    bool vsTightPassive = (opponentType_ == OpponentType::TightPassive);

    // EXPLOIT: Value bet thinner vs stations, normal vs LAG, tighter vs tight
    double thinValueThreshold = vsCallingStation ? 0.40 : (vsLAG ? 0.55 : 0.60);
    double bluffFrequency = vsTightPassive ? 0.40 : 0.25;

    // EXPLOIT: Don't bluff vs LAG or stations - they call too much
    if (vsLAG || vsCallingStation) {
        bluffFrequency = 0.0;
    }

    // EXPLOIT: Value bet HARD vs LAG (they call too wide)
    // Use smaller sizing to keep them calling with worse
    if (vsLAG && equity > 0.55) {
        int64_t betSize = static_cast<int64_t>(pot_ * 0.50);
        return Decision::bet(betSize, "Value bet vs LAG - they call wide");
    }

    // Value bets - bet bigger with stronger hands
    if (equity > 0.80) {
        int64_t betSize = static_cast<int64_t>(pot_ * 0.75);
        return Decision::bet(betSize, "Big value bet with very strong hand");
    }

    if (equity > thinValueThreshold) {
        int64_t betSize = static_cast<int64_t>(pot_ * 0.60);
        return Decision::bet(betSize, "Value bet with strong hand");
    }

    // Semi-bluffs with draws
    int outs = HandEvaluator::countOuts(heroCards_, board_);
    if (outs >= 12) {
        int64_t betSize = static_cast<int64_t>(pot_ * 0.60);
        return Decision::bet(betSize, std::format("Semi-bluff with {} outs", outs));
    }
    if (outs >= 6) {
        int64_t betSize = static_cast<int64_t>(pot_ * 0.40);
        return Decision::bet(betSize, std::format("Probe bet with {} outs", outs));
    }

    // Continuation bet - but not vs LAGs
    if (!vsLAG && equity > 0.50) {
        int64_t betSize = static_cast<int64_t>(pot_ * 0.50);
        return Decision::bet(betSize, "Continuation bet with equity advantage");
    }

    // Thin value vs calling stations
    if (vsCallingStation && equity > 0.38) {
        int64_t betSize = static_cast<int64_t>(pot_ * 0.40);
        return Decision::bet(betSize, "Thin value bet vs calling station");
    }

    // Even weak hands should sometimes bet (bluff) in HU
    // EXPLOIT: Bluff more vs tight players, never vs stations/LAG
    if (!vsLAG && !vsCallingStation && equity > 0.35) {
        int64_t betSize = static_cast<int64_t>(pot_ * 0.33);
        return Decision::bet(betSize, "Small bet for thin value/protection");
    }

    // EXPLOITATIVE bluffs based on opponent
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if ((vsTightPassive || opponentType_ == OpponentType::Random) &&
        dist(rng_) < bluffFrequency) {
        int64_t betSize = static_cast<int64_t>(pot_ * 0.33);
        return Decision::bet(betSize, "Exploitative bluff vs tight opponent");
    }

    return Decision::check("Check with garbage");
}

void Simulation::settleShowdown() {
    CardSet heroFull, villainFull;

    for (size_t i = 0; i < heroCards_.count; ++i) heroFull.add(heroCards_.cards[i]);
    for (size_t i = 0; i < villainCards_.count; ++i) villainFull.add(villainCards_.cards[i]);
    for (size_t i = 0; i < board_.count; ++i) {
        heroFull.add(board_.cards[i]);
        villainFull.add(board_.cards[i]);
    }

    ::sharkwave::HandResult heroHand = HandEvaluator::evaluate(heroFull);
    ::sharkwave::HandResult villainHand = HandEvaluator::evaluate(villainFull);

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

    // Track starting stacks for profit calculation
    int64_t heroStartStack = heroStack_;

    // Cap stacks at 200 BB to simulate real poker (you'd leave table or cash out)
    const int64_t maxStack = 200 * bb_;
    if (heroStack_ > maxStack) heroStack_ = maxStack;
    if (villainStack_ > maxStack) villainStack_ = maxStack;
    // Rebuy if below 20 BB
    const int64_t minStack = 100 * bb_;
    if (heroStack_ < minStack) heroStack_ = minStack;
    if (villainStack_ < minStack) villainStack_ = minStack;

    // Reset for new hand
    pot_ = 0;

    // Heads up: Hero is BTN/SB, Villain is BB
    heroPosition_ = Position::BTN;
    villainPosition_ = Position::BB;

    // Post blinds
    heroStack_ -= sb_;
    villainStack_ -= bb_;
    pot_ = sb_ + bb_;

    int64_t heroInvested = sb_;
    int64_t villainInvested = bb_;

    // Deal hole cards
    dealHoleCards();

    bool handOver = false;
    bool heroWon = false;
    bool reachedShowdown = false;

    int64_t currentBet = 0;  // Highest bet in current street
    int64_t heroBetThisStreet = 0;
    int64_t villainBetThisStreet = 0;

    // === PREFLOP ===
    // Hero (SB/BTN) acts first in heads up
    currentBet = bb_;
    heroBetThisStreet = sb_;
    villainBetThisStreet = bb_;

    Decision heroDecision = getHeroDecision(false, 0);

    if (heroDecision.action == Action::Fold) {
        handOver = true;
        heroWon = false;
        villainStack_ += pot_;
        pot_ = 0;
    } else if (heroDecision.action == Action::Raise) {
        int64_t raiseAmt = heroDecision.amount;
        int64_t toCall = raiseAmt - heroBetThisStreet;

        heroStack_ -= toCall;
        pot_ += toCall;
        heroInvested += toCall;
        heroBetThisStreet = raiseAmt;
        currentBet = raiseAmt;

        // Villain responds
        int64_t villainFacing = currentBet - villainBetThisStreet;
        Action villainAction = getOpponentAction(villainPosition_, villainCards_, villainFacing, false);

        if (villainAction == Action::Fold) {
            handOver = true;
            heroWon = true;
            heroStack_ += pot_;
            pot_ = 0;
        } else if (villainAction == Action::Call) {
            villainStack_ -= villainFacing;
            pot_ += villainFacing;
            villainInvested += villainFacing;
            villainBetThisStreet = currentBet;
        } else if (villainAction == Action::Raise) {
            int64_t threebet = currentBet + bb_; // Minimum 3-bet
            int64_t villainToCall = threebet - villainBetThisStreet;

            villainStack_ -= villainToCall;
            pot_ += villainToCall;
            villainInvested += villainToCall;
            currentBet = threebet;
            villainBetThisStreet = threebet;

            // Hero responds to 3-bet - simplify by calling
            int64_t heroFacing = currentBet - heroBetThisStreet;
            heroStack_ -= heroFacing;
            pot_ += heroFacing;
            heroInvested += heroFacing;
            heroBetThisStreet = currentBet;
        }
    }

    // Helper for postflop betting
    auto runBettingRound = [&](bool, bool, bool) -> bool {
        if (handOver) return true;

        currentBet = 0;
        heroBetThisStreet = 0;
        villainBetThisStreet = 0;

        // Villain (OOP in BB) acts first
        Action villainAction = getOpponentAction(villainPosition_, villainCards_, 0, true);

        if (villainAction == Action::Bet || villainAction == Action::Raise) {
            int64_t betAmt = (pot_ * 2) / 3; // ~66% pot
            if (betAmt > villainStack_) betAmt = villainStack_;

            villainStack_ -= betAmt;
            pot_ += betAmt;
            villainInvested += betAmt;
            villainBetThisStreet = betAmt;
            currentBet = betAmt;

            // Hero responds
            Decision heroDecision = getHeroDecision(true, currentBet - heroBetThisStreet);

            if (heroDecision.action == Action::Fold) {
                handOver = true;
                heroWon = false;
                villainStack_ += pot_;
                pot_ = 0;
                return true;
            } else if (heroDecision.action == Action::Call) {
                int64_t callAmt = currentBet - heroBetThisStreet;
                heroStack_ -= callAmt;
                pot_ += callAmt;
                heroInvested += callAmt;
                heroBetThisStreet = currentBet;
            } else if (heroDecision.action == Action::Raise) {
                int64_t raiseAmt = currentBet * 2;
                if (raiseAmt > heroStack_) raiseAmt = heroStack_;
                int64_t toCall = raiseAmt - heroBetThisStreet;

                heroStack_ -= toCall;
                pot_ += toCall;
                heroInvested += toCall;
                currentBet = raiseAmt;
                heroBetThisStreet = raiseAmt;

                // Villain responds to raise - simplify by calling or folding
                int64_t villainFacing = currentBet - villainBetThisStreet;
                Action vResponse = getOpponentAction(villainPosition_, villainCards_, villainFacing, false);

                if (vResponse == Action::Fold) {
                    handOver = true;
                    heroWon = true;
                    heroStack_ += pot_;
                    pot_ = 0;
                    return true;
                } else if (vResponse == Action::Call) {
                    villainStack_ -= villainFacing;
                    pot_ += villainFacing;
                    villainInvested += villainFacing;
                    villainBetThisStreet = currentBet;
                }
            }
        } else {
            // Villain checks
            // Hero acts
            Decision heroDecision = getHeroDecision(false, 0);

            if (heroDecision.action == Action::Bet) {
                int64_t betAmt = heroDecision.amount;
                if (betAmt > heroStack_) betAmt = heroStack_;

                heroStack_ -= betAmt;
                pot_ += betAmt;
                heroInvested += betAmt;
                heroBetThisStreet = betAmt;
                currentBet = betAmt;

                // Villain responds
                int64_t villainFacing = currentBet - villainBetThisStreet;
                Action vResponse = getOpponentAction(villainPosition_, villainCards_, villainFacing, false);

                if (vResponse == Action::Fold) {
                    handOver = true;
                    heroWon = true;
                    heroStack_ += pot_;
                    pot_ = 0;
                    return true;
                } else if (vResponse == Action::Call) {
                    villainStack_ -= villainFacing;
                    pot_ += villainFacing;
                    villainInvested += villainFacing;
                    villainBetThisStreet = currentBet;
                } else if (vResponse == Action::Raise) {
                    int64_t raiseAmt = currentBet * 2;
                    if (raiseAmt > villainStack_) raiseAmt = villainStack_;
                    int64_t villainToCall = raiseAmt - villainBetThisStreet;

                    villainStack_ -= villainToCall;
                    pot_ += villainToCall;
                    villainInvested += villainToCall;
                    currentBet = raiseAmt;
                    villainBetThisStreet = raiseAmt;

                    // Hero responds - simplify by calling
                    int64_t heroFacing = currentBet - heroBetThisStreet;
                    heroStack_ -= heroFacing;
                    pot_ += heroFacing;
                    heroInvested += heroFacing;
                    heroBetThisStreet = currentBet;
                }
            }
        }

        return false;
    };

    // === FLOP ===
    if (!handOver) {
        dealFlop();
        handOver = runBettingRound(true, false, false);
    }

    // === TURN ===
    if (!handOver) {
        dealTurn();
        handOver = runBettingRound(false, true, false);
    }

    // === RIVER ===
    if (!handOver) {
        dealRiver();
        handOver = runBettingRound(false, false, true);
    }

    // === SHOWDOWN ===
    if (!handOver) {
        reachedShowdown = true;
        settleShowdown();
        heroWon = (heroStack_ > heroStartStack);
    }

    // Calculate profit (stack change from start)
    int64_t profit = heroStack_ - heroStartStack;

    return SimHandResult{heroWon, profit, reachedShowdown, heroWon};
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
            // Won by opponent folding preflop or postflop
            wins++;
        } else {
            // We folded - lose the chips we put in
            losses++;
        }

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
    std::cout << "Showdowns:        " << showdownCount_ << "\n";

    std::cout << "\n";
    std::cout << "Total profit:     " << heroTotalProfit_ << " chips\n";
    std::cout << "Profit/100 hands: " << (100.0 * heroTotalProfit_ / numHands_) << " chips\n";
    std::cout << "BB/100:           " << (heroTotalProfit_ / (double)numHands_ / bb_ * 100) << "\n";
    std::cout << "ROI:              " << (100.0 * heroTotalProfit_ / (numHands_ * 1000.0)) << "%\n";
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
