#include "card.h"
#include "game_session.h"
#include "decision_engine.h"
#include "hand_evaluator.h"

#include <iostream>
#include <format>
#include <string>
#include <cctype>
#include <iomanip>

using namespace sharkwave;

// CLI helpers
void printHeader() {
    std::cout << "\n=== SHARKWAVE ===\n";
    std::cout << "New session. Let's make money.\n\n";
}

void printStreetHeader(Street street) {
    std::cout << "\n=== " << GameSession::streetToString(street) << " === ";
}

void printPotInfo(GameSession& session) {
    std::cout << "Pot: " << session.pot() << "\n";
}

void printDecision(const Decision& decision) {
    std::cout << "\n> DO THIS: ";
    switch (decision.action) {
        case Action::Fold:
            std::cout << "FOLD";
            break;
        case Action::Check:
            std::cout << "CHECK";
            break;
        case Action::Call:
            std::cout << "CALL " << decision.amount;
            break;
        case Action::Bet:
            std::cout << "BET " << decision.amount;
            break;
        case Action::Raise:
            std::cout << "RAISE to " << decision.amount;
            break;
    }
    if (!decision.reason.empty()) {
        std::cout << "\n> WHY: " << decision.reason;
    }
    std::cout << "\n";
}

void printYourTurn() {
    std::cout << "\nYOUR TURN.\n";
}

void printGameInfo(GameSession& session) {
    // Show hand description postflop
    if (session.street() >= Street::Flop && session.board().count >= 3) {
        std::string handDesc = HandEvaluator::describeHand(session.heroCards(), session.board());
        std::cout << "> Hand: " << handDesc << "\n";
    }

    // Show SPR (Stack-to-Pot Ratio) - crucial for decision making
    double spr = session.spr();
    int64_t effectiveStack = session.effectiveStack();
    int64_t pot = session.pot();

    std::cout << "> SPR: " << std::fixed << std::setprecision(1) << spr;
    std::cout << " (Effective stack: " << effectiveStack << ", Pot: " << pot << ")\n";

    // Show pot odds if facing a bet
    if (session.toCall() > 0) {
        double potOdds = session.potOdds();
        int64_t toCall = session.toCall();
        double oddsPercent = potOdds * 100.0;
        std::cout << "> Facing bet: " << toCall << " to win " << (pot + toCall);
        std::cout << " (" << std::fixed << std::setprecision(1) << oddsPercent << "% pot odds)\n";
    }
}

Position parsePosition(const std::string& input) {
    std::string upper = input;
    for (char& c : upper) c = std::toupper(c);

    if (upper == "UTG") return Position::UTG;
    if (upper == "MP")  return Position::MP;
    if (upper == "CO")  return Position::CO;
    if (upper == "BTN" || upper == "BUTTON") return Position::BTN;
    if (upper == "SB")  return Position::SB;
    if (upper == "BB")  return Position::BB;

    return Position::CO; // Default
}

Card parseCard(const std::string& input) {
    if (input.length() < 2) {
        return Card(Rank::Two, Suit::Clubs); // Default
    }

    char rankChar = std::toupper(input[0]);
    char suitChar = std::tolower(input[1]);

    Rank rank;
    switch (rankChar) {
        case '2': rank = Rank::Two;   break;
        case '3': rank = Rank::Three; break;
        case '4': rank = Rank::Four;  break;
        case '5': rank = Rank::Five;  break;
        case '6': rank = Rank::Six;   break;
        case '7': rank = Rank::Seven; break;
        case '8': rank = Rank::Eight; break;
        case '9': rank = Rank::Nine;  break;
        case 'T': rank = Rank::Ten;   break;
        case 'J': rank = Rank::Jack;  break;
        case 'Q': rank = Rank::Queen; break;
        case 'K': rank = Rank::King;  break;
        case 'A': rank = Rank::Ace;   break;
        default:  rank = Rank::Two;   break;
    }

    Suit suit;
    switch (suitChar) {
        case 'c': suit = Suit::Clubs;    break;
        case 'd': suit = Suit::Diamonds; break;
        case 'h': suit = Suit::Hearts;   break;
        case 's': suit = Suit::Spades;   break;
        default:  suit = Suit::Clubs;    break;
    }

    return Card(rank, suit);
}

void runSession() {
    GameSession session;

    printHeader();

    // Session setup
    int players = 6;
    int64_t stack = 1000;
    int sb = 5, bb = 10;

    std::cout << "Players? [" << players << "] ";
    std::string line;
    std::getline(std::cin, line);
    if (!line.empty()) players = std::stoi(line);
    session.setPlayerCount(players);

    std::cout << "Your stack? [" << stack << "] ";
    std::getline(std::cin, line);
    if (!line.empty()) stack = std::stoi(line);
    session.setHeroStack(stack);

    std::cout << "Blinds? [" << sb << "/" << bb << "] ";
    std::getline(std::cin, line);
    if (!line.empty()) {
        size_t slash = line.find('/');
        if (slash != std::string::npos) {
            sb = std::stoi(line.substr(0, slash));
            bb = std::stoi(line.substr(slash + 1));
        }
    }
    session.setBlinds(sb, bb);

    while (true) {
        // New hand
        session.newHand();

        std::cout << "\n========================================\n";
        std::cout << "NEW HAND\n";
        std::cout << "========================================\n";

        // Get position
        std::cout << "Your position? (UTG/MP/CO/BTN/SB/BB) ";
        std::getline(std::cin, line);
        Position pos = parsePosition(line);
        session.setHeroPosition(pos);

        // Get hole cards
        std::cout << "Your cards? (e.g. Jc Tc) ";
        std::getline(std::cin, line);
        if (line.length() >= 4) {
            Card c1 = parseCard(line.substr(0, 2));
            Card c2 = parseCard(line.substr(line.length() - 2));
            session.setHeroCards(c1, c2);
        }

        // Preflop
        DecisionEngine engine(session);
        printStreetHeader(Street::Preflop);
        printPotInfo(session);
        printYourTurn();
        printGameInfo(session);

        Decision decision = engine.makeDecision();
        printDecision(decision);

        std::cout << "\nYour action? (fold/check/call/bet/raise or \"done\" to continue) ";
        std::getline(std::cin, line);

        bool handOver = false;
        int64_t profitThisHand = 0;

        if (line.empty() || line[0] == 'f' || line[0] == 'F') {
            // User folded
            handOver = true;
            std::cout << "(Folded -" << (bb / 2) << " chips)\n";
            profitThisHand = -(bb / 2);
        } else if (line == "done" || line == "skip") {
            // Continue to flop
        } else if (line[0] == 'r' || line[0] == 'R' || line[0] == 'b' || line[0] == 'B') {
            // User bet or raised - continue to flop
            std::cout << "(Bet/raise - continue to flop)\n";
        }

        // Ask about opponent actions before flop
        if (!handOver) {
            std::cout << "\nOpponents? (actions like \"call fold\" or \"done\" if all folded) ";
            std::getline(std::cin, line);
            if (line == "done" || line.find("all fold") != std::string::npos) {
                // All opponents folded - we win preflop
                std::cout << "(You won the blinds!)\n";
                profitThisHand = sb + bb;
                handOver = true;
            }
        }

        if (!handOver) {
            // Flop
            printStreetHeader(Street::Flop);
            printPotInfo(session);

            std::cout << "Board? (e.g. 9c 8c 2d or \"skip\") ";
            std::getline(std::cin, line);
            if (line != "skip" && line.length() >= 6) {
                Card c1 = parseCard(line.substr(0, 2));
                Card c2 = parseCard(line.substr(3, 5));
                Card c3 = parseCard(line.substr(6, 8));
                session.setFlop(c1, c2, c3);

                printYourTurn();
        printGameInfo(session);
                decision = engine.makeDecision();
                printDecision(decision);

                std::cout << "\nYour action? (fold/check/call/bet/raise or \"done\") ";
                std::getline(std::cin, line);

                if (line.empty() || line[0] == 'f' || line[0] == 'F') {
                    handOver = true;
                    std::cout << "(Folded)\n";
                }
            } else {
                handOver = true;
            }

            session.nextStreet();
        }

        if (!handOver) {
            // Turn
            printStreetHeader(Street::Turn);
            printPotInfo(session);

            std::cout << "Turn? (e.g. 3h or \"skip\") ";
            std::getline(std::cin, line);
            if (line != "skip" && !line.empty() && line.length() >= 2) {
                Card c = parseCard(line);
                session.setTurn(c);

                printYourTurn();
        printGameInfo(session);
                decision = engine.makeDecision();
                printDecision(decision);

                std::cout << "\nYour action? (fold/check/call/bet/raise or \"done\") ";
                std::getline(std::cin, line);

                if (line.empty() || line[0] == 'f' || line[0] == 'F') {
                    handOver = true;
                    std::cout << "(Folded)\n";
                }
            } else {
                handOver = true;
            }

            session.nextStreet();
        }

        if (!handOver) {
            // River
            printStreetHeader(Street::River);
            printPotInfo(session);

            std::cout << "River? (e.g. Qc or \"skip\") ";
            std::getline(std::cin, line);
            if (line != "skip" && !line.empty() && line.length() >= 2) {
                Card c = parseCard(line);
                session.setRiver(c);

                printYourTurn();
        printGameInfo(session);
                decision = engine.makeDecision();
                printDecision(decision);

                std::cout << "\nYour action? (fold/check/call/bet/raise or \"done\") ";
                std::getline(std::cin, line);

                if (line.empty() || line[0] == 'f' || line[0] == 'F') {
                    handOver = true;
                    std::cout << "(Folded)\n";
                }
            } else {
                handOver = true;
            }
        }

        // Showdown/result
        if (!handOver) {
            std::cout << "\n=== SHOWDOWN ===\n";
            std::cout << "Result? (+chips won/-chips lost, or 0 for loss) ";
            std::getline(std::cin, line);
            if (!line.empty()) {
                try {
                    profitThisHand = std::stoll(line);
                    std::cout << "(Hand " << (profitThisHand >= 0 ? "won" : "lost") << ": " << profitThisHand << " chips)\n";
                } catch (...) {
                    profitThisHand = 0;
                }
            }
        }

        // Continue?
        std::cout << "\nNext hand? (y/n) ";
        std::getline(std::cin, line);
        if (line.empty() || std::tolower(line[0]) != 'y') {
            break;
        }
    }

    std::cout << "\nSession complete.\n";
    std::cout << "Hands played: " << session.handsPlayed() << "\n";
    std::cout << "Final profit: " << session.sessionProfit() << "\n";
}

int main() {
    try {
        runSession();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
