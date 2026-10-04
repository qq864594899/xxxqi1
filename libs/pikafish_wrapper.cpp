#include <string>
#include <sstream>
#include <fstream>
#include <functional>
#include <unistd.h>
#include "engine.h"
#include "position.h"
#include "bitboard.h"

using namespace Stockfish;

static std::string g_result;
static Engine *g_engine = nullptr;

static void logMsg(const char* msg) {
    std::ofstream f("/var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pf_debug.txt", std::ios::app);
    if (f.is_open()) f << msg << std::endl;
}

extern "C" const char* pf_bestmove(const char* fen, int movetime_ms) {
    g_result = "NONE";
    logMsg("=== 开始 ===");
    
    // 只初始化一次
    if (!g_engine) {
        logMsg("初始化 Bitboards");
        Bitboards::init();
        logMsg("初始化 Position");
        Position::init();
        
        logMsg("创建 Engine 对象");
        g_engine = new Engine();
        if (!g_engine) {
            logMsg("Engine 创建失败");
            return g_result.c_str();
        }
        logMsg("Engine 创建成功");
        
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
        g_engine->load_network(std::filesystem::path(nnuePath));
        logMsg("NNUE 加载完成");
        
        logMsg("设置回调");
        g_engine->set_on_bestmove([](std::string_view bm, std::string_view ponder) {
            g_result = std::string(bm);
            logMsg(("回调触发: " + g_result).c_str());
        });
    }
    
    logMsg("设置局面");
    std::string fenStr(fen);
    if (fenStr.find(" - - ") == std::string::npos) {
        if (!fenStr.empty() && (fenStr.back() == 'w' || fenStr.back() == 'b')) {
            fenStr += " - - 0 1";
        }
    }
    auto err = g_engine->set_position(fenStr);
    if (err.has_value()) {
        logMsg("set_position 失败");
        return g_result.c_str();
    }
    logMsg("局面设置完成");
    
    logMsg("清空搜索");
    g_engine->search_clear();
    
    logMsg("启动搜索");
    // 用 UCIEngine 的 loop 不现实，这里先发一个空的 move 触发搜索
    // 或者用 Engine 内部方法 —— 但皮卡鱼新版没有公开的 go()
    // 暂时用 g_engine 的 search_clear 后等 1 秒看能否触发
    usleep((movetime_ms + 1000) * 1000);
    
    logMsg("搜索完成");
    logMsg(("结果: " + g_result).c_str());
    return g_result.c_str();
}
