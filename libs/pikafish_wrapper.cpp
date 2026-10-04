#include <string>
#include <sstream>
#include <iostream>
#include <utility>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include "engine.h"
#include "position.h"
#include "thread.h"
#include "tt.h"
#include "bitboard.h"
#include "search.h"

using namespace Stockfish;

static void logMsg(const char* msg) {
    std::ofstream f("/var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pf_debug.txt", std::ios::app);
    if (f.is_open()) f << msg << std::endl;
}

static std::string g_result;

extern "C" const char* pf_bestmove(const char* fen, int movetime_ms) {
    g_result = "NONE";
    logMsg("=== 开始 ===");
    
    try {
        logMsg("初始化 Bitboards");
        Bitboards::init();
        logMsg("初始化 Position");
        Position::init();
        logMsg("初始化 Threads");
        Threads.init();
        logMsg("初始化 TT");
        TT.resize(16);
        logMsg("初始化完成");
        
        logMsg("检查 NNUE 文件");
        std::string nnuePath = "/var/mobile/xiangqiassist/pikafish.nnue";
        std::ifstream test(nnuePath);
        if (!test.good()) {
            logMsg("NNUE 文件不存在！");
            g_result = "NO_NNUE";
            return g_result.c_str();
        }
        test.close();
        logMsg("NNUE 文件存在");
        
        logMsg("加载 NNUE");
        Engine::load_network(std::filesystem::path(nnuePath));
        logMsg("NNUE 加载完成");
        
        logMsg("设置局面");
        std::string fenStr(fen);
        if (fenStr.find(" - - ") == std::string::npos) {
            fenStr += " - - 0 1";
        }
        auto err = Engine::set_position(fenStr);
        if (err.has_value()) {
            logMsg("set_position 失败");
            g_result = "ERR_POS";
            return g_result.c_str();
        }
        logMsg("局面设置完成");
        
        logMsg("设置回调");
        Engine::set_on_bestmove([](std::string_view bm, std::string_view ponder) {
            g_result = std::string(bm);
            logMsg(("bestmove 回调: " + g_result).c_str());
        });
        
        logMsg("清空搜索");
        Engine::search_clear();
        
        logMsg("启动搜索");
        Search::LimitsType limits;
        limits.movetime = movetime_ms;
        Threads.start_thinking(limits);
        
        logMsg("等待搜索完成");
        usleep((movetime_ms + 500) * 1000);
        Threads.main()->wait_for_search_finished();
        logMsg("搜索完成");
        
    } catch (const std::exception& e) {
        logMsg(("异常: " + std::string(e.what())).c_str());
        g_result = "EXCEPTION";
    } catch (...) {
        logMsg("未知异常");
        g_result = "UNKNOWN";
    }
    
    logMsg(("最终结果: " + g_result).c_str());
    return g_result.c_str();
}
