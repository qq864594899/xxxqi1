#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include "uci.h"
#include "misc.h"

using namespace Stockfish;

// ===== 日志 =====
static void logMsg(const char* msg) {
    std::ofstream f("/var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pf_debug.txt", std::ios::app);
    if (f.is_open()) f << msg << std::endl;
}

extern "C" const char* pf_bestmove(const char* fen, int movetime_ms) {
    static std::string result = "NONE";
    logMsg("=== 开始 ===");
    
    try {
        // 构造 UCI 命令串
        std::string input;
        input += "setoption name EvalFile value /var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pikafish.nnue\n";
        input += "position fen " + std::string(fen) + " - - 0 1\n";
        input += "go movetime " + std::to_string(movetime_ms) + "\n";
        
        logMsg("准备重定向");
        std::istringstream in(input);
        std::ostringstream out;
        std::streambuf* oldCin = std::cin.rdbuf(in.rdbuf());
        std::streambuf* oldCout = std::cout.rdbuf(out.rdbuf());
        
        logMsg("创建 UCIEngine");
        int fake_argc = 1;
        char* fake_argv[] = {(char*)"pikafish", NULL};
        CommandLine cli(fake_argc, fake_argv);
        UCIEngine engine(std::move(cli));
        
        logMsg("调用 loop");
        engine.loop();
        logMsg("loop 返回");
        
        // 恢复流
        std::cin.rdbuf(oldCin);
        std::cout.rdbuf(oldCout);
        
        // 从输出里找 bestmove
        std::string s = out.str();
        size_t p = s.find("bestmove ");
        if (p != std::string::npos) {
            p += 9;
            int i = 0;
            while (p + i < s.length() && s[p+i] != ' ' && s[p+i] != '\n' && i < 5) i++;
            result = s.substr(p, i);
        }
        
        logMsg(("结果: " + result).c_str());
        
    } catch (const std::exception& e) {
        logMsg(("异常: " + std::string(e.what())).c_str());
        return "EXCEPTION";
    } catch (...) {
        logMsg("未知异常");
        return "UNKNOWN";
    }
    
    return result.c_str();
}
