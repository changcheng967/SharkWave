#include "gui.h"
#include <windows.h>

using namespace sharkwave;

// Debug file accessible from gui.cpp
FILE* g_debugFile = nullptr;

static void DebugLog(const char* msg) {
    if (g_debugFile) {
        fprintf(g_debugFile, "%s\n", msg);
        fflush(g_debugFile);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Open debug file directly (no stdout/stderr redirection)
    fopen_s(&g_debugFile, "sharkwave_debug.txt", "w");
    DebugLog("=== SharkWave GUI Started ===");

    PokerGui gui;
    gui.run();

    if (g_debugFile) {
        DebugLog("=== SharkWave GUI Exiting ===");
        fclose(g_debugFile);
        g_debugFile = nullptr;
    }

    return 0;
}
