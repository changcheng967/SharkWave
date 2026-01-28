#pragma once

#include <string>
#include <functional>
#include "card.h"
#include "game_session.h"

namespace sharkwave {

enum class Language {
    English,
    Chinese
};

struct GuiState {
    Language language = Language::English;

    // Session settings
    int playerCount = 6;
    int heroStack = 1000;
    int sb = 5;
    int bb = 10;
    Position heroPosition = Position::CO;

    // Cards
    std::string cardInput1 = "";  // First card
    std::string cardInput2 = "";  // Second card
    std::string boardInput = "";   // Board cards

    // Opponent info
    int64_t facingBet = 0;
    bool isPreflop = true;

    // Results
    std::string decisionOutput = "";
    std::string reasonOutput = "";
    std::string equityOutput = "";
    std::string sprOutput = "";
    std::string handDescOutput = "";

    // Session tracking
    int64_t sessionProfit = 0;
    int handsPlayed = 0;

    // Debug log
    std::string debugLog = "";

    // Current street
    Street currentStreet = Street::Preflop;
};

class PokerGui {
public:
    PokerGui();
    void run();

    // Public methods for WndProc access
    void makeDecision();
    void setLanguage(Language lang);
    void logDebug(const std::string& msg);
    void logDebug(const char* fmt, ...);
    GuiState& state() { return state_; }

private:
    void init();
    void render();
    void update();
    void processInput();

    // Language
    std::string t(const char* english, const char* chinese);

    // Card parsing
    bool parseCardInput(const std::string& input, Card& card);

    GuiState state_;
    bool running_ = true;
};

// Global for WndProc access
extern PokerGui* g_gui;

} // namespace sharkwave
