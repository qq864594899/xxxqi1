#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <mach/mach.h>
#import <mach/vm_region.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

static BOOL started = NO;
extern char **environ;
// ========== 日志 ==========
static void writeLog(NSString *msg) {
    NSString *path = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents/xiangqi_log.txt"];
    NSString *line = [NSString stringWithFormat:@"%@\n", msg];
    NSLog(@"[XQ] %@", msg);
    NSFileHandle *fh = [NSFileHandle fileHandleForWritingAtPath:path];
    if (!fh) {
        [line writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil];
    } else {
        [fh seekToEndOfFile];
        [fh writeData:[line dataUsingEncoding:NSUTF8StringEncoding]];
        [fh closeFile];
    }
}

// ========== 窗口 ==========
static UIWindow *getAnyWindow(void) {
    for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
        if ([scene isKindOfClass:[UIWindowScene class]]) {
            NSArray *wins = ((UIWindowScene *)scene).windows;
            for (UIWindow *w in wins) if (w.isKeyWindow) return w;
            if (wins.count > 0) return wins.lastObject;
        }
    }
    return nil;
}

// ========== 内存扫描找 FEN ==========
static NSString *scanFenFromMemory(void) {
    vm_address_t addr = 0;
    vm_size_t size = 0;
    natural_t depth = 0;

    NSString *bestFen = nil;
    int bestSteps = -1;

    const char *needle = "rnbakabnr";
    size_t needleLen = 9;

    int regionsScanned = 0;

    while (1) {
        struct vm_region_submap_info_64 info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;

        kern_return_t kr = vm_region_recurse_64(
            mach_task_self(), &addr, &size, &depth,
            (vm_region_info_t)&info, &count
        );
        if (kr != KERN_SUCCESS) break;

        if ((info.protection & VM_PROT_READ) && (info.protection & VM_PROT_WRITE) && size < 200 * 1024 * 1024) {
            vm_offset_t data = 0;
            mach_msg_type_number_t dataSize = 0;

            kr = vm_read(mach_task_self(), addr, size, &data, &dataSize);
            if (kr == KERN_SUCCESS && data && dataSize > 0) {
                regionsScanned++;
                char *base = (char *)data;
                char *p = base;
                char *end = base + dataSize;

                while (p && p < end) {
                    void *found = memmem(p, end - p, needle, needleLen);
                    if (!found) break;

                    char *fp = (char *)found;
            
                    char *nul = (char *)memchr(fp, 0, end - fp);
                    size_t len = nul ? (size_t)(nul - fp) : (size_t)(end - fp);

                    if (len > 20 && len < 2000) {
                        char *buf = (char *)malloc(len + 1);
                        if (buf) {
                            memcpy(buf, fp, len);
                            buf[len] = 0;

                            NSString *s = [NSString stringWithUTF8String:buf];
                            free(buf);

                            if (s) {
                                NSRange movesRange = [s rangeOfString:@"moves"];
                                if (movesRange.location != NSNotFound) {
                                    NSString *after = [s substringFromIndex:movesRange.location + 5];
                                    NSArray *tokens = [after componentsSeparatedByString:@" "];
                                    int steps = 0;
                                    for (NSString *t in tokens) {
                                        if (t.length == 4) {
                                            unichar c0 = [t characterAtIndex:0];
                                            unichar c1 = [t characterAtIndex:1];
                                            unichar c2 = [t characterAtIndex:2];
                                            unichar c3 = [t characterAtIndex:3];
                                            if (c0 >= 'a' && c0 <= 'i' && c1 >= '0' && c1 <= '9' &&
                                                c2 >= 'a' && c2 <= 'i' && c3 >= '0' && c3 <= '9') {
                                                steps++;
                                            }
                                        }
                                    }
                                    if (steps > bestSteps) {
                                        bestSteps = steps;
                                        bestFen = s;
                                    }
                                }
                            }
                        }
                    }
                    p = (char *)found + needleLen;
                }

                vm_deallocate(mach_task_self(), data, dataSize);
            }
        }

        addr += size;
        if (addr == 0 || size == 0) break;
    }

    writeLog([NSString stringWithFormat:@"扫描了 %d 个区域", regionsScanned]);

    if (!bestFen) return nil;

    NSRange movesRange = [bestFen rangeOfString:@"moves"];
    if (movesRange.location == NSNotFound) return nil;

    NSString *head = [bestFen substringToIndex:movesRange.location];
    NSString *after = [bestFen substringFromIndex:movesRange.location + 5];
    NSArray *tokens = [after componentsSeparatedByString:@" "];
    NSMutableArray *validMoves = [NSMutableArray array];
    for (NSString *t in tokens) {
        if (t.length == 4) {
            unichar c0 = [t characterAtIndex:0];
            unichar c1 = [t characterAtIndex:1];
            unichar c2 = [t characterAtIndex:2];
            unichar c3 = [t characterAtIndex:3];
            if (c0 >= 'a' && c0 <= 'i' && c1 >= '0' && c1 <= '9' &&
                c2 >= 'a' && c2 <= 'i' && c3 >= '0' && c3 <= '9') {
                [validMoves addObject:t];
            }
        }
    }

    return [NSString stringWithFormat:@"%@moves %@", head, [validMoves componentsJoinedByString:@" "]];
}

// ========== 调用皮卡鱼（posix_spawn） ==========
static NSString *runPikafish(NSString *fen, int movetimeMs) {
    NSString *binPath = @"/var/mobile/xiangqiassist/pikafish";
    NSString *nnuePath = @"/var/mobile/xiangqiassist/pikafish.nnue";

    if (![[NSFileManager defaultManager] fileExistsAtPath:binPath]) {
        return @"引擎不存在";
    }

    int inPipe[2];
    int outPipe[2];
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

    void (^send)(NSString *) = ^(NSString *cmd) {
        NSString *line = [cmd stringByAppendingString:@"\n"];
        const char *c = [line UTF8String];
        write(wfd, c, strlen(c));
    };

    send(@"uci");
    send([NSString stringWithFormat:@"setoption name EvalFile value %@", nnuePath]);
    send([NSString stringWithFormat:@"position fen %@", fen]);
    send([NSString stringWithFormat:@"go movetime %d", movetimeMs]);

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

// ========== 主逻辑 ==========
static void runInference(void) {
    writeLog(@"=== 开始扫描 ===");

    NSDate *t0 = [NSDate date];
    NSString *fen = scanFenFromMemory();
    NSTimeInterval dt = -[t0 timeIntervalSinceNow];
    writeLog([NSString stringWithFormat:@"扫描耗时: %.2f 秒", dt]);

    if (!fen || fen.length == 0) {
        writeLog(@"未找到 FEN");
        return;
    }
    writeLog([NSString stringWithFormat:@"FEN: %@", fen]);

    NSDate *t1 = [NSDate date];
    NSString *bm = runPikafish(fen, 1000);
    NSTimeInterval dt2 = -[t1 timeIntervalSinceNow];
    writeLog([NSString stringWithFormat:@"引擎耗时: %.2f 秒", dt2]);
    writeLog([NSString stringWithFormat:@"引擎建议: %@", bm]);
}

// ========== UI ==========
static UIView *panel = nil;
static UILabel *resultLabel = nil;

@interface XQController : NSObject
- (void)onDetect:(UIButton *)sender;
@end

@implementation XQController
- (void)onDetect:(UIButton *)sender {
    [sender setTitle:@"识别中..." forState:UIControlStateNormal];
    [sender setEnabled:NO];
    resultLabel.text = @"扫描中...";

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        runInference();

        dispatch_async(dispatch_get_main_queue(), ^{
            [sender setTitle:@"识别" forState:UIControlStateNormal];
            [sender setEnabled:YES];
            resultLabel.text = @"结果写日志";
        });
    });
}
@end

static XQController *ctl = nil;

static void createPanel(void) {
    if (panel) return;
    if (!ctl) ctl = [[XQController alloc] init];
    UIWindow *window = getAnyWindow();
    if (!window) return;

    panel = [[UIView alloc] initWithFrame:CGRectMake(window.bounds.size.width - 200, 120, 180, 110)];
    panel.backgroundColor = [[UIColor blackColor] colorWithAlphaComponent:0.85];
    panel.layer.cornerRadius = 10;

    UIButton *btn = [UIButton buttonWithType:UIButtonTypeSystem];
    btn.frame = CGRectMake(10, 10, 160, 40);
    [btn setTitle:@"识别" forState:UIControlStateNormal];
    [btn setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    btn.backgroundColor = [UIColor systemBlueColor];
    btn.layer.cornerRadius = 8;
    [btn addTarget:ctl action:@selector(onDetect:) forControlEvents:UIControlEventTouchUpInside];
    [panel addSubview:btn];

    resultLabel = [[UILabel alloc] initWithFrame:CGRectMake(10, 55, 160, 45)];
    resultLabel.text = @"结果写日志";
    resultLabel.numberOfLines = 2;
    resultLabel.textColor = [UIColor whiteColor];
    resultLabel.font = [UIFont systemFontOfSize:11];
    [panel addSubview:resultLabel];

    [window addSubview:panel];
}

%hook UIViewController
- (void)viewDidAppear:(BOOL)animated {
    %orig;
    if (started) return;
    started = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        createPanel();
    });
}
%end
