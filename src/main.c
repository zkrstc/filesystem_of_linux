#include "../include/ext2.h"
#include "../include/commands.h"
#include "../include/user.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

// 信号处理函数
void signal_handler(int sig) {
    printf("\nReceived signal %d, cleaning up...\n", sig);
    ext2_cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    (void)argc; // 避免未使用参数警告
    (void)argv;
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 初始化随机数种子
    srand(time(NULL));
    
    // 初始化文件系统
    if (ext2_init(NULL) != 0) {
        printf("Error: Failed to initialize file system\n");
        return 1;
    }
    
    // 显示欢迎信息
    printf("========================================\n");
    printf("    EXT2 File System Simulator\n");
    printf("========================================\n");
    printf("This is a simplified EXT2 file system implementation\n");
    printf("Features:\n");
    printf("- File and directory operations\n");
    printf("- User authentication and permissions\n");
    printf("- Inode-based file management\n");
    printf("- Block allocation and bitmap management\n");
    printf("- Multi-level directory structure\n");
    printf("========================================\n\n");
    
    // 启动命令循环
    command_loop();
    
    // 清理资源
    ext2_cleanup();
    
    printf("Goodbye!\n");
    return 0;
} 