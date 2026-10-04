#include <string>
#include <sstream>
#include <iostream>
#include <utility>
#include <fstream>
#include <unistd.h>
#include "engine.h"
#include "position.h"
#include "thread.h"
#include "tt.h"
#include "bitboard.h"
#include "search.h"

using namespace Stockfish;

static std::string g_result;
static bool g_inited = false;

static void logMsg(const char* msg) {
    std::ofstream f("/var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pf_debug.txt", std::ios::app);
    if (f.is_open()) f << msg << std::endl;
}

extern "C" const char* pf_bestmove(const char* fen, int movetime_ms) {
    g_result = "NONE";
    logMsg("=== 开始 ===");
    
    if (!g_inited) {
        logMsg("初始化 Attacks");
        Bitboards::init();
        Position::init();
        Threads.init();
        TT.resize(16);
        g_inited = true;
        logMsg("初始化完成");
    }
    
    logMsg("检查 NNUE");
    std::string nnuePath = "/var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pikafish.nnue";
    std::ifstream test(nnuePath);
    if (!test.good()) {
        logMsg("NNUE 不存在");
        return g_result.c_str();
    }
    test.close();
    logMsg("NNUE 存在");
    
    logMsg("加载 NNUE");
    Engine::load_network(std::filesystem::path(nnuePath));
    logMsg("NNUE 加载完成");
    
    logMsg("设置回调");
    Engine::set_on_bestmove([](std::string_view bm, std::string_view ponder) {
        g_result = std::string(bm);
        logMsg(("回调触发: " + g_result).c_str());
    });
    
    logMsg("设置局面");
    std::string fenStr(fen);
    if (fenStr.find(" - - ") == std::string::npos) {
        if (!fenStr.empty() && (fenStr.back() == 'w' || fenStr.back() == 'b')) {
            fenStr += " - - 0 1";
        }
    }
    auto err = Engine::set_position(fenStr);
    if (err.has_value()) {
        logMsg("set_position 失败");
        return g_result.c_str();
    }
    logMsg("局面设置完成");
    
    logMsg("清空搜索");
    Engine::search_clear();
    
    logMsg("启动搜索");
    Search::LimitsType limits;
    limits.movetime = movetime_ms;
    Threads.start_thinking(limits);
    
    logMsg("等待搜索完成");
    usleep((movetime_ms + 1000) * 1000);
    Threads.main()->wait_for_search_finished();
    logMsg("搜索完成");
    
    logMsg(("结果: " + g_result).c_str());
    return g_result.c_str();
}
