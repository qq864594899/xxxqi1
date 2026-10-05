#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

extern char **environ;

static NSString *runPikafish(NSString *fen, int movetimeMs) {
    NSString *binPath = @"/var/mobile/xiangqiassist/pikafish";
    NSString *nnuePath = @"/var/mobile/xiangqiassist/pikafish.nnue";

    if (![[NSFileManager defaultManager] fileExistsAtPath:binPath]) {
        return @"引擎不存在";
    }

    int inPipe[2];   // 我们写，引擎读
    int outPipe[2];  // 引擎写，我们读
    if (pipe(inPipe) != 0 || pipe(outPipe) != 0) return @"pipe 失败";

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, inPipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, inPipe[1]);
    posix_spawn_file_actions_addclose(&actions, outPipe[0]);

    pid_t pid = 0;
    const char *argv[] = { [binPath UTF8String], NULL };
    int ret = posix_spawn(&pid, [binPath UTF8String], &actions, NULL, (char *const *)argv, environ);
    posix_spawn_file_actions_destroy(&actions);

    if (ret != 0) {
        close(inPipe[0]); close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        return [NSString stringWithFormat:@"spawn 失败: %d", ret];
    }

    close(inPipe[0]);
    close(outPipe[1]);

    int wfd = inPipe[1];
    int rfd = outPipe[0];

    // 发命令的辅助函数
    void (^send)(NSString *) = ^(NSString *cmd) {
        NSString *line = [cmd stringByAppendingString:@"\n"];
        const char *c = [line UTF8String];
        write(wfd, c, strlen(c));
    };

    send(@"uci");
    // 等 uciok（简化处理，直接发后面的命令，引擎会排队）
    send(@"setoption name EvalFile value /var/mobile/xiangqiassist/pikafish.nnue");
    send([NSString stringWithFormat:@"position fen %@", fen]);
    send([NSString stringWithFormat:@"go movetime %d", movetimeMs]);

    // 读输出，找 bestmove
    NSMutableString *output = [NSMutableString string];
    char buf[4096];
    NSString *bestmove = nil;
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:(movetimeMs / 1000.0) + 10.0];

    while ([deadline timeIntervalSinceNow] > 0) {
        ssize_t n = read(rfd, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = 0;
        NSString *chunk = [NSString stringWithUTF8String:buf];
        if (chunk) {
            [output appendString:chunk];
            NSRange r = [output rangeOfString:@"bestmove "];
            if (r.location != NSNotFound) {
                NSString *rest = [output substringFromIndex:r.location + 9];
                NSArray *parts = [rest componentsSeparatedByString:@" "];
                if (parts.count > 0) {
                    bestmove = parts[0];
                    break;
                }
            }
        }
    }

    send(@"quit");
    close(wfd);
    close(rfd);

    int status = 0;
    waitpid(pid, &status, 0);

    return bestmove ?: @"超时";
}
