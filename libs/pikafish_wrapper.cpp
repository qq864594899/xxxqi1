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
#include "bitboards.h"
#include "search.h"

using namespace Stockfish;

// ===== 全局变量，让回调能改到 =====
static std::string g_result;
static bool g_inited = false;

// ===== 日志 =====
static void logMsg(const char* msg) {
    std::ofstream f("/var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pf_debug.txt", std::ios::app);
    if (f.is_open()) f << msg << std::endl;
}

extern "C" const char* pf_bestmove(const char* fen, int movetime_ms) {
    g_result = "NONE";
    logMsg("=== 开始 ===");
    
    try {
        // ===== 初始化 =====
        if (!g_inited) {
            logMsg("初始化 Attacks");
            Bitboards::init();
            logMsg("初始化 Position");
            Position::init();
            logMsg("初始化 Threads");
            Threads.init();
            logMsg("初始化 TT");
            TT.resize(16);
            g_inited = true;
            logMsg("初始化完成");
        }
        
        // ===== 检查 NNUE =====
        logMsg("检查 NNUE");
        std::string nnuePath = "/var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pikafish.nnue";
        std::ifstream test(nnuePath);
        if (!test.good()) {
            logMsg("NNUE 不存在");
            return "NO_NNUE";
        }
        test.close();
        
        // ===== 加载 NNUE =====
        logMsg("加载 NNUE");
        Engine::load_network(std::filesystem::path(nnuePath));
        logMsg("NNUE 加载完成");
        
        // ===== 设置回调（写全局变量） =====
        logMsg("设置回调");
        Engine::set_on_bestmove([](std::string_view bm, std::string_view ponder) {
            g_result = std::string(bm);
            logMsg(("回调触发！bestmove: " + g_result).c_str());
        });
        
        // ===== 设置局面 =====
        logMsg("设置局面");
        std::string fenStr(fen);
        if (fenStr.find(" - - ") == std::string::npos) {
            fenStr += " - - 0 1";
        }
        auto err = Engine::set_position(fenStr);
        if (err.has_value()) {
            logMsg("set_position 失败");
            return "ERR_POS";
        }
        logMsg("局面设置完成");
        
        // ===== 清空搜索 =====
        logMsg("清空搜索");
        Engine::search_clear();
        
        // ===== 启动搜索 =====
        logMsg("启动搜索");
        Search::LimitsType limits;
        limits.movetime = movetime_ms;
        Threads.start_thinking(limits);
        
        // ===== 等待搜索完成 =====
        logMsg("等待搜索完成");
        usleep((movetime_ms + 1000) * 1000);
        Threads.main()->wait_for_search_finished();
        logMsg("搜索完成");
        
        logMsg(("结果: " + g_result).c_str());
        
    } catch (const std::exception& e) {
        logMsg(("异常: " + std::string(e.what())).c_str());
        return "EXCEPTION";
    } catch (...) {
        logMsg("未知异常");
        return "UNKNOWN";
    }
    
    return g_result.c_str();
}
