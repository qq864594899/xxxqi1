#include <string>
#include <sstream>
#include <iostream>
#include <utility>
#include <fstream>
#include "uci.h"
#include "misc.h"

using namespace Stockfish;

static void logMsg(const char* msg) {
    std::ofstream f("/var/mobile/Containers/Data/Application/8E8A56AB-971B-4CF3-85F4-6F2A380D26B7/Documents/pf_debug.txt", std::ios::app);
    if (f.is_open()) f << msg << std::endl;
}

extern "C" const char* pf_bestmove(const char* fen, int movetime_ms) {
    static std::string result = "NONE";
    logMsg("pf_bestmove 开始");
    
    std::string input;
    input += "setoption name EvalFile value /var/mobile/xiangqiassist/pikafish.nnue\n";
    input += "position fen " + std::string(fen) + " - - 0 1\n";
    input += "go movetime " + std::to_string(movetime_ms) + "\n";
    logMsg("输入构造完成");
    
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldCin = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldCout = std::cout.rdbuf(out.rdbuf());
    logMsg("流重定向完成");
    
    try {
        int fake_argc = 1;
        char* fake_argv[] = {(char*)"pikafish", NULL};
        logMsg("创建 CommandLine");
        CommandLine cli(fake_argc, fake_argv);
        logMsg("创建 UCIEngine");
        UCIEngine engine(std::move(cli));
        logMsg("调用 engine.loop()");
        engine.loop();
        logMsg("engine.loop() 返回");
    } catch (const std::exception& e) {
        logMsg("异常");
        result = "EXCEPTION";
    } catch (...) {
        logMsg("未知异常");
        result = "UNKNOWN_EXCEPTION";
    }
    
    std::cin.rdbuf(oldCin);
    std::cout.rdbuf(oldCout);
    
    std::string s = out.str();
    logMsg("输出长度检查");
    size_t p = s.find("bestmove ");
    if (p != std::string::npos) {
        p += 9;
        int i = 0;
        while (p + i < s.length() && s[p+i] != ' ' && s[p+i] != '\n' && i < 5) i++;
        result = s.substr(p, i);
    }
    logMsg(result.c_str());
    return result.c_str();
}
