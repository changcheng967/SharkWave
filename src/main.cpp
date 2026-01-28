#include "card.h"
#include "game_session.h"
#include "decision_engine.h"
#include "hand_evaluator.h"

#include <iostream>
#include <format>
#include <string>
#include <cctype>

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

        Decision decision = engine.makeDecision();
        printDecision(decision);

        std::cout << "\nOpponents? (enter actions like \"fold fold call\" or \"done\") ";
        std::getline(std::cin, line);

        if (line == "done" || line == "skip") {
            // Skip to flop
            session.nextStreet();
        } else {
            // Process actions (simplified for MVP)
            session.nextStreet();
        }

        // Flop
        printStreetHeader(Street::Flop);
        printPotInfo(session);

        std::cout << "Board? (e.g. 9c 8c 2d) ";
        std::getline(std::cin, line);
        if (line.length() >= 6) {
            Card c1 = parseCard(line.substr(0, 2));
            Card c2 = parseCard(line.substr(3, 5));
            Card c3 = parseCard(line.substr(6, 8));
            session.setFlop(c1, c2, c3);
        }

        printYourTurn();
        decision = engine.makeDecision();
        printDecision(decision);

        std::cout << "\nOpponents? (enter actions or \"done\") ";
        std::getline(std::cin, line);

        if (line == "done" || line == "skip") {
            session.nextStreet();
        } else {
            session.nextStreet();
        }

        // Turn
        printStreetHeader(Street::Turn);
        printPotInfo(session);

        std::cout << "Turn? (e.g. 3h) ";
        std::getline(std::cin, line);
        if (!line.empty() && line.length() >= 2) {
            Card c = parseCard(line);
            session.setTurn(c);
        }

        printYourTurn();
        decision = engine.makeDecision();
        printDecision(decision);

        std::cout << "\nOpponents? (enter actions or \"done\") ";
        std::getline(std::cin, line);

        if (line == "done" || line == "skip") {
            session.nextStreet();
        } else {
            session.nextStreet();
        }

        // River
        printStreetHeader(Street::River);
        printPotInfo(session);

        std::cout << "River? (e.g. Qc) ";
        std::getline(std::cin, line);
        if (!line.empty() && line.length() >= 2) {
            Card c = parseCard(line);
            session.setRiver(c);
        }

        printYourTurn();
        decision = engine.makeDecision();
        printDecision(decision);

        // Showdown/result
        std::cout << "\n=== SHOWDOWN ===\n";
        std::cout << "Result? (win/loss/tie amount) ";
        std::getline(std::cin, line);

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
