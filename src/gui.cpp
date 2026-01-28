#include "gui.h"
#include "decision_engine.h"
#include "hand_evaluator.h"
#include "gto_charts.h"

#include <windows.h>
#include <commctrl.h>
#include <cstdarg>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

// Debug file shared with main_gui.cpp
extern FILE* g_debugFile;

namespace sharkwave {

// Localization strings
const char* STR_APP_NAME[] = { "SharkWave - GTO Poker Assistant", "SharkWave - GTO扑克助手" };
const char* STR_SETTINGS[] = { "SETTINGS", "设置" };
const char* STR_PLAYERS[] = { "Players:", "玩家人数:" };
const char* STR_STACK[] = { "Your Stack:", "你的筹码:" };
const char* STR_BLINDS[] = { "Blinds:", "盲注:" };
const char* STR_POSITION[] = { "Your Position:", "你的位置:" };
const char* STR_CARDS[] = { "YOUR CARDS", "你的手牌" };
const char* STR_CARD1[] = { "Card 1 (e.g. Ah):", "第一张牌 (如Ah):" };
const char* STR_CARD2[] = { "Card 2 (e.g. Kh):", "第二张牌 (如Kh):" };
const char* STR_BOARD[] = { "BOARD (flop/turn/river)", "公共牌 (翻牌/转牌/河牌)" };
const char* STR_FACING[] = { "Facing Bet:", "面对下注:" };
const char* STR_DECIDE[] = { "GET DECISION", "获取决策" };
const char* STR_NEXT_HAND[] = { "NEXT HAND", "下一手" };
const char* STR_DECISION[] = { "DECISION", "决策" };
const char* STR_REASON[] = { "REASON", "原因" };
const char* STR_EQUITY[] = { "EQUITY", "胜率" };
const char* STR_SPR[] = { "SPR", "底池比" };
const char* STR_HAND[] = { "HAND", "牌力" };
const char* STR_STATS[] = { "SESSION STATS", "战绩统计" };
const char* STR_HANDS[] = { "Hands:", "手数:" };
const char* STR_PROFIT[] = { "Profit:", "盈利:" };
const char* STR_LANGUAGE[] = { "Language / 语言", "Language / 语言" };
const char* STR_ENGLISH[] = { "English", "英文" };
const char* STR_CHINESE[] = { "中文", "中文" };
const char* STR_DEBUG[] = { "DEBUG LOG", "调试日志" };
const char* STR_FOLD[] = { "FOLD", "弃牌" };
const char* STR_CHECK[] = { "CHECK", "过牌" };
const char* STR_CALL[] = { "CALL", "跟注" };
const char* STR_BET[] = { "BET", "下注" };
const char* STR_RAISE[] = { "RAISE", "加注" };

// Window handles
HWND g_hWnd = nullptr;
HWND g_hCard1Edit = nullptr;
HWND g_hCard2Edit = nullptr;
HWND g_hBoardEdit = nullptr;
HWND g_hDecisionLabel = nullptr;
HWND g_hDecisionText = nullptr;
HWND g_hReasonLabel = nullptr;
HWND g_hReasonText = nullptr;
HWND g_hEquityLabel = nullptr;
HWND g_hEquityText = nullptr;
HWND g_hSprLabel = nullptr;
HWND g_hSprText = nullptr;
HWND g_hHandLabel = nullptr;
HWND g_hHandText = nullptr;
HWND g_hStatsLabel = nullptr;
HWND g_hDebugEdit = nullptr;
HWND g_hDecideButton = nullptr;
HWND g_hNextButton = nullptr;
HWND g_hLangButton = nullptr;

PokerGui* g_gui = nullptr;
HINSTANCE g_hInstance = nullptr;

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void appendDebug(const char* text);
void appendDebug(const wchar_t* text);

// Position names
const char* POSITION_NAMES_EN[] = { "UTG", "MP", "CO", "BTN", "SB", "BB" };
const char* POSITION_NAMES_CN[] = { "枪口", "中位", "关煞", "按钮", "小盲", "大盲" };

std::string PokerGui::t(const char* english, const char* chinese) {
    return (state_.language == Language::Chinese) ? std::string(chinese) : std::string(english);
}

void PokerGui::setLanguage(Language lang) {
    state_.language = lang;
    logDebug("Language changed to: %s", (lang == Language::English) ? "English" : "Chinese");
}

void PokerGui::logDebug(const std::string& msg) {
    state_.debugLog += msg + "\n";
    // Also write to debug file
    if (g_debugFile) {
        fprintf(g_debugFile, "%s\n", msg.c_str());
        fflush(g_debugFile);
    }
    if (g_hDebugEdit) {
        appendDebug(msg.c_str());
    }
}

void PokerGui::logDebug(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    logDebug(std::string(buffer));
}

bool PokerGui::parseCardInput(const std::string& input, Card& card) {
    logDebug("Parsing card input: '%s'", input.c_str());

    if (input.length() < 2) {
        logDebug("ERROR: Input too short");
        return false;
    }

    char rankChar = input[0];
    char suitChar = input[1];

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
        case 'T': case 't': rank = Rank::Ten;   break;
        case 'J': case 'j': rank = Rank::Jack;  break;
        case 'Q': case 'q': rank = Rank::Queen; break;
        case 'K': case 'k': rank = Rank::King;  break;
        case 'A': case 'a': rank = Rank::Ace;   break;
        default:
            logDebug("ERROR: Invalid rank char '%c'", rankChar);
            return false;
    }

    Suit suit;
    switch (suitChar) {
        case 'c': suit = Suit::Clubs;    break;
        case 'd': suit = Suit::Diamonds; break;
        case 'h': suit = Suit::Hearts;   break;
        case 's': suit = Suit::Spades;   break;
        case 'C': suit = Suit::Clubs;    break;
        case 'D': suit = Suit::Diamonds; break;
        case 'H': suit = Suit::Hearts;   break;
        case 'S': suit = Suit::Spades;   break;
        default:
            logDebug("ERROR: Invalid suit char '%c'", suitChar);
            return false;
    }

    card = Card(rank, suit);
    logDebug("PARSED: Rank=%d Suit=%d", (int)rank, (int)suit);
    return true;
}

void PokerGui::makeDecision() {
    logDebug("=== MAKING DECISION ===");

    // Get card inputs
    char card1Buf[16] = {}, card2Buf[16] = {}, boardBuf[64] = {};
    GetWindowTextA(g_hCard1Edit, card1Buf, 16);
    GetWindowTextA(g_hCard2Edit, card2Buf, 16);
    GetWindowTextA(g_hBoardEdit, boardBuf, 64);

    state_.cardInput1 = card1Buf;
    state_.cardInput2 = card2Buf;
    state_.boardInput = boardBuf;

    logDebug("Card1 input: '%s'", card1Buf);
    logDebug("Card2 input: '%s'", card2Buf);
    logDebug("Board input: '%s'", boardBuf);

    // Parse cards
    Card c1, c2;
    if (!parseCardInput(state_.cardInput1, c1) || !parseCardInput(state_.cardInput2, c2)) {
        std::string err = t("ERROR: Invalid card format. Use format like 'Ah', 'Ks'", "错误: 牌格式无效。使用如'Ah', 'Ks'格式");
        SetWindowTextA(g_hReasonText, err.c_str());
        return;
    }

    // Create session
    GameSession session;
    session.setPlayerCount(state_.playerCount);
    session.setHeroStack(state_.heroStack);
    session.setBlinds(state_.sb, state_.bb);
    session.setHeroPosition(state_.heroPosition);
    session.setHeroCards(c1, c2);

    logDebug("Session created: pos=%d stack=%d sb=%d bb=%d",
        (int)state_.heroPosition, (int)state_.heroStack, state_.sb, state_.bb);

    // Parse board if provided
    if (!state_.boardInput.empty() && state_.boardInput != "skip") {
        logDebug("Parsing board...");
        std::string b = state_.boardInput;
        if (b.length() >= 6) {
            Card flop1, flop2, flop3;
            if (parseCardInput(b.substr(0, 2), flop1) &&
                parseCardInput(b.substr(3, 5), flop2) &&
                parseCardInput(b.substr(6, 8), flop3)) {
                session.setFlop(flop1, flop2, flop3);
                logDebug("Flop set");

                // Turn?
                if (b.length() >= 11) {
                    Card turn;
                    if (parseCardInput(b.substr(9, 11), turn)) {
                        session.setTurn(turn);
                        logDebug("Turn set");
                    }
                }

                // River?
                if (b.length() >= 14) {
                    Card river;
                    if (parseCardInput(b.substr(12, 14), river)) {
                        session.setRiver(river);
                        logDebug("River set");
                    }
                }
            }
        }
    }

    // Get decision
    DecisionEngine engine(session);
    Decision decision = engine.makeDecision();

    logDebug("Decision: action=%d amount=%d", (int)decision.action, (int)decision.amount);

    // Format output
    std::string actionStr;
    switch (decision.action) {
        case Action::Fold: actionStr = t(STR_FOLD[0], STR_FOLD[1]); break;
        case Action::Check: actionStr = t(STR_CHECK[0], STR_CHECK[1]); break;
        case Action::Call: actionStr = t(STR_CALL[0], STR_CALL[1]); break;
        case Action::Bet: actionStr = t(STR_BET[0], STR_BET[1]); break;
        case Action::Raise: actionStr = t(STR_RAISE[0], STR_RAISE[1]); break;
    }

    if (decision.amount > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), " %lld", (long long)decision.amount);
        actionStr += buf;
    }

    state_.decisionOutput = actionStr;
    state_.reasonOutput = decision.reason;

    // Calculate equity
    double equity = HandEvaluator::calculateEquity(session.heroCards(), session.board(), 500);
    char eqBuf[32];
    snprintf(eqBuf, sizeof(eqBuf), "%.1f%%", equity * 100.0);
    state_.equityOutput = eqBuf;

    // SPR
    double spr = session.spr();
    char sprBuf[32];
    snprintf(sprBuf, sizeof(sprBuf), "%.1f", spr);
    state_.sprOutput = sprBuf;

    // Hand description
    if (session.board().count >= 3) {
        state_.handDescOutput = HandEvaluator::describeHand(session.heroCards(), session.board());
    } else {
        state_.handDescOutput = t("Preflop", "翻牌前");
    }

    // Update UI
    SetWindowTextA(g_hDecisionText, state_.decisionOutput.c_str());
    SetWindowTextA(g_hReasonText, state_.reasonOutput.c_str());
    SetWindowTextA(g_hEquityText, state_.equityOutput.c_str());
    SetWindowTextA(g_hSprText, state_.sprOutput.c_str());
    SetWindowTextA(g_hHandText, state_.handDescOutput.c_str());

    logDebug("=== DECISION COMPLETE ===");
    logDebug("Action: %s", actionStr.c_str());
    logDebug("Reason: %s", decision.reason.c_str());
    logDebug("Equity: %s", eqBuf);
    logDebug("SPR: %s", sprBuf);
}

PokerGui::PokerGui() {
    logDebug("PokerGui constructor called");
}

void appendDebug(const char* text) {
    if (!g_hDebugEdit) return;

    int len = GetWindowTextLengthA(g_hDebugEdit);
    SendMessageA(g_hDebugEdit, EM_SETSEL, len, len);
    SendMessageA(g_hDebugEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageA(g_hDebugEdit, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
}

void appendDebug(const wchar_t* text) {
    // Convert to UTF-8 and append
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        char* buffer = new char[size];
        WideCharToMultiByte(CP_UTF8, 0, text, -1, buffer, size, nullptr, nullptr);
        appendDebug(buffer);
        delete[] buffer;
    }
}

void PokerGui::init() {
    logDebug("Initializing GUI...");
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            g_gui->logDebug("WM_CREATE - Window created");

            // Create fonts
            HFONT hTitleFont = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                DEFAULT_PITCH | FF_SWISS, "Arial");
            HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                DEFAULT_PITCH | FF_SWISS, "Arial");

            // Title label
            CreateWindowExA(0, "STATIC", STR_APP_NAME[0],
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 10, 760, 40, hWnd, (HMENU)1, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 1, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

            // Language button
            g_hLangButton = CreateWindowExA(0, "BUTTON", STR_ENGLISH[0],
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                600, 50, 180, 30, hWnd, (HMENU)100, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 100, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Settings section
            CreateWindowExA(0, "STATIC", STR_SETTINGS[0],
                WS_CHILD | WS_VISIBLE, 20, 90, 200, 20, hWnd, (HMENU)2, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 2, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Cards section
            CreateWindowExA(0, "STATIC", STR_CARDS[0],
                WS_CHILD | WS_VISIBLE, 20, 180, 200, 20, hWnd, (HMENU)3, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 3, WM_SETFONT, (WPARAM)hFont, TRUE);

            CreateWindowExA(0, "STATIC", STR_CARD1[0],
                WS_CHILD | WS_VISIBLE, 20, 210, 150, 20, hWnd, (HMENU)4, g_hInstance, NULL);
            g_hCard1Edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                180, 210, 100, 22, hWnd, (HMENU)5, g_hInstance, NULL);

            CreateWindowExA(0, "STATIC", STR_CARD2[0],
                WS_CHILD | WS_VISIBLE, 20, 240, 150, 20, hWnd, (HMENU)6, g_hInstance, NULL);
            g_hCard2Edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                180, 240, 100, 22, hWnd, (HMENU)7, g_hInstance, NULL);

            // Board section
            CreateWindowExA(0, "STATIC", STR_BOARD[0],
                WS_CHILD | WS_VISIBLE, 20, 280, 300, 20, hWnd, (HMENU)8, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 8, WM_SETFONT, (WPARAM)hFont, TRUE);
            g_hBoardEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                20, 310, 260, 22, hWnd, (HMENU)9, g_hInstance, NULL);

            // Decision button
            g_hDecideButton = CreateWindowExA(0, "BUTTON", STR_DECIDE[0],
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                300, 310, 140, 35, hWnd, (HMENU)10, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 10, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Next hand button
            g_hNextButton = CreateWindowExA(0, "BUTTON", STR_NEXT_HAND[0],
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                460, 310, 140, 35, hWnd, (HMENU)11, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 11, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Decision output section
            CreateWindowExA(0, "STATIC", STR_DECISION[0],
                WS_CHILD | WS_VISIBLE, 20, 360, 150, 25, hWnd, (HMENU)12, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 12, WM_SETFONT, (WPARAM)hFont, TRUE);
            g_hDecisionText = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_READONLY,
                20, 390, 580, 30, hWnd, (HMENU)13, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 13, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Reason
            CreateWindowExA(0, "STATIC", STR_REASON[0],
                WS_CHILD | WS_VISIBLE, 20, 430, 150, 25, hWnd, (HMENU)14, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 14, WM_SETFONT, (WPARAM)hFont, TRUE);
            g_hReasonText = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY,
                20, 460, 580, 50, hWnd, (HMENU)15, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 15, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Stats row
            CreateWindowExA(0, "STATIC", STR_EQUITY[0],
                WS_CHILD | WS_VISIBLE, 20, 525, 80, 20, hWnd, (HMENU)16, g_hInstance, NULL);
            g_hEquityText = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_READONLY,
                100, 525, 80, 22, hWnd, (HMENU)17, g_hInstance, NULL);

            CreateWindowExA(0, "STATIC", STR_SPR[0],
                WS_CHILD | WS_VISIBLE, 200, 525, 50, 20, hWnd, (HMENU)18, g_hInstance, NULL);
            g_hSprText = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_READONLY,
                260, 525, 80, 22, hWnd, (HMENU)19, g_hInstance, NULL);

            CreateWindowExA(0, "STATIC", STR_HAND[0],
                WS_CHILD | WS_VISIBLE, 360, 525, 60, 20, hWnd, (HMENU)20, g_hInstance, NULL);
            g_hHandText = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_READONLY,
                420, 525, 180, 22, hWnd, (HMENU)21, g_hInstance, NULL);

            // Session stats
            CreateWindowExA(0, "STATIC", STR_STATS[0],
                WS_CHILD | WS_VISIBLE, 20, 560, 150, 25, hWnd, (HMENU)22, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 22, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Debug log
            CreateWindowExA(0, "STATIC", STR_DEBUG[0],
                WS_CHILD | WS_VISIBLE, 20, 600, 150, 20, hWnd, (HMENU)23, g_hInstance, NULL);
            g_hDebugEdit = CreateWindowExA(WS_EX_CLIENTEDGE | WS_VSCROLL, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                20, 625, 580, 150, hWnd, (HMENU)24, g_hInstance, NULL);
            SendDlgItemMessage(hWnd, 24, WM_SETFONT, (WPARAM)hFont, TRUE);

            g_gui->logDebug("=== GUI INITIALIZED ===");
            g_gui->logDebug("All controls created");
            g_gui->logDebug("Ready for input");
            break;
        }

        case WM_COMMAND: {
            // Only log button clicks, not every notification
            if (HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == 10) {  // Decide button
                    g_gui->logDebug("DECIDE button clicked");
                    g_gui->makeDecision();
                }
                else if (LOWORD(wParam) == 11) {  // Next hand button
                    g_gui->logDebug("NEXT HAND button clicked");
                    SetWindowTextA(g_hCard1Edit, "");
                    SetWindowTextA(g_hCard2Edit, "");
                    SetWindowTextA(g_hBoardEdit, "");
                    SetWindowTextA(g_hDecisionText, "");
                    SetWindowTextA(g_hReasonText, "");
                    SetWindowTextA(g_hEquityText, "");
                    SetWindowTextA(g_hSprText, "");
                    SetWindowTextA(g_hHandText, "");
                }
                else if (LOWORD(wParam) == 100) {  // Language button
                    g_gui->logDebug("LANGUAGE button clicked");
                    g_gui->setLanguage((g_gui->state().language == Language::English) ?
                        Language::Chinese : Language::English);
                }
            }
            break;
        }

        case WM_DESTROY: {
            g_gui->logDebug("WM_DESTROY - Window closing");
            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void PokerGui::run() {
    logDebug("PokerGui::run() starting");

    g_hInstance = GetModuleHandle(NULL);
    g_gui = this;

    const char* CLASS_NAME = "SharkWaveWindow";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    logDebug("Registering window class: %s", CLASS_NAME);
    if (!RegisterClassA(&wc)) {
        logDebug("ERROR: RegisterClass failed");
        return;
    }

    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    int winWidth = 800;
    int winHeight = 850;

    logDebug("Creating window: %dx%d", winWidth, winHeight);
    HWND hWnd = CreateWindowExA(
        0, CLASS_NAME, STR_APP_NAME[0], style,
        100, 50, winWidth, winHeight,
        NULL, NULL, g_hInstance, NULL
    );

    if (!hWnd) {
        logDebug("ERROR: CreateWindow failed");
        return;
    }

    g_hWnd = hWnd;
    logDebug("Window created successfully: HWND=%p", (void*)hWnd);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    logDebug("Window shown");

    // Message loop
    MSG msg = {};
    logDebug("Entering message loop...");
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    logDebug("Message loop ended");
    running_ = false;
}

} // namespace sharkwave
