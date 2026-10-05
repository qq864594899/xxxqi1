#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <unistd.h>
#include "engine.h"
#include "position.h"
#include "bitboard.h"
#include "uci.h"

using namespace Stockfish;

static std::string g_result;
static UCIEngine *g_uci = nullptr;
static std::istringstream *g_cin = nullptr;
static std::ostringstream *g_cout = nullptr;

extern "C" const char* pf_bestmove(const char* fen, int movetime_ms) {
    g_result = "NONE";

    if (!g_uci) {
        Attacks::init();
        Position::init();

        g_cin = new std::istringstream();
        g_cout = new std::ostringstream();

        g_uci = new UCIEngine(0, nullptr);
        // 注意：UCIEngine 的 loop 会从 std::cin 读，需要重定向
    }

    // 把命令写进 cin 流
    std::string fenStr(fen);
    if (fenStr.find(" - - ") == std::string::npos) {
        if (!fenStr.empty() && (fenStr.back() == 'w' || fenStr.back() == 'b')) {
            fenStr += " - - 0 1";
        }
    }

    // 组合 UCI 命令
    std::string cmd = "position fen " + fenStr + "\ngo movetime " + std::to_string(movetime_ms) + "\n";
    
    // 这里需要把 cmd 喂给 UCIEngine::loop，但 loop 是阻塞的
    // 更实际的做法是直接用 Engine 的公开接口

    return g_result.c_str();
}
