#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include "engine.h"
#include "attacks.h"
#include "position.h"
#include "search.h"
#include "tt.h"

using namespace Stockfish;

static void logMsg(const char* msg) {
    std::ofstream f("/var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pf_debug.txt", std::ios::app);
    if (f.is_open()) f << msg << std::endl;
}

static std::string g_result;
static Engine* g_engine = nullptr;

extern "C" const char* pf_bestmove(const char* fen, int movetime_ms) {
    g_result = "NONE";
    logMsg("=== 开始 ===");
    
    try {
        logMsg("初始化 Attacks");
        Attacks::init();
        logMsg("初始化完成");
        
        // 只创建一次 Engine 对象
        if (!g_engine) {
            logMsg("创建 Engine 对象");
            // Engine 构造函数需要参数，我们看情况
            // 先试无参构造
            g_engine = new Engine();
            logMsg("Engine 创建成功");
        }
        
        logMsg("检查 NNUE");
        std::string nnuePath = "/var/mobile/xiangqiassist/pikafish.nnue";
        std::ifstream test(nnuePath);
        if (!test.good()) {
            logMsg("NNUE 不存在");
            g_result = "NO_NNUE";
            return g_result.c_str();
        }
        test.close();
        
        logMsg("加载 NNUE");
        g_engine->load_network(nnuePath);
        logMsg("NNUE 加载完成");
        
        logMsg("设置局面");
        std::string fenStr(fen);
        if (fenStr.find(" - - ") == std::string::npos) {
            fenStr += " - - 0 1";
        }
        auto err = g_engine->set_position(fenStr);
        if (err.has_value()) {
            logMsg("set_position 失败");
            g_result = "ERR_POS";
            return g_result.c_str();
        }
        logMsg("局面设置完成");
        
        logMsg("设置回调");
        g_engine->set_on_bestmove([](std::string_view bm, std::string_view ponder) {
            g_result = std::string(bm);
            logMsg(("bestmove: " + g_result).c_str());
        });
        
        logMsg("清空搜索");
        g_engine->search_clear();
        
        logMsg("引擎状态就绪");
        
    } catch (const std::exception& e) {
        logMsg(("异常: " + std::string(e.what())).c_str());
        g_result = "EXCEPTION";
    } catch (...) {
        logMsg("未知异常");
        g_result = "UNKNOWN";
    }
    
    logMsg(("结果: " + g_result).c_str());
    return g_result.c_str();
}
