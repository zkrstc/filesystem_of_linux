#ifndef COMMANDS_H
#define COMMANDS_H

#include "ext2.h"

// 文件操作命令
int cmd_create(const char *path);
int cmd_delete(const char *path);
int cmd_open(const char *path, int flags);
int cmd_close(int fd);
int cmd_read(int fd, void *buffer, size_t size);
int cmd_write(int fd, const void *buffer, size_t size);

// 目录操作命令
int cmd_dir(const char *path);
int cmd_mkdir(const char *path);
int cmd_rmdir(const char *path);
int cmd_cd(const char *path);

// 用户操作命令
int cmd_login(const char *username, const char *password);
int cmd_logout(void);
int cmd_users(void);

// 文件系统管理命令
int cmd_format(const char *disk_image);
int cmd_mount(const char *disk_image);
int cmd_umount(void);
int cmd_status(void);

// 权限管理命令
int cmd_chmod(const char *path, uint16_t mode);
int cmd_chown(const char *path, uint16_t uid, uint16_t gid);

// 特权命令
int cmd_useradd(const char *username, const char *password, uint16_t uid, uint16_t gid);

// 帮助命令
void cmd_help(void);
void print_usage(void);

// 命令解析
int parse_command(char *line);
void command_loop(void);

void get_cwd_path(char *buf, size_t size);

#endif // COMMANDS_H 