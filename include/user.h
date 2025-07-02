#ifndef USER_H
#define USER_H

#include "ext2.h"

// 用户管理
int init_users(void);
int add_user(const char *username, const char *password, uint16_t uid, uint16_t gid);
int remove_user(const char *username);
int find_user(const char *username);

// 用户认证
int login(const char *username, const char *password);
void logout(void);
int is_logged_in(void);

// 权限检查
int check_file_permission(uint32_t inode_no, int access);
int check_directory_permission(uint32_t inode_no, int access);

// 当前用户信息
uint16_t get_current_uid(void);
uint16_t get_current_gid(void);
const char* get_current_username(void);

// 用户列表
void list_users(void);

// 密码管理
int change_password(const char *username, const char *old_password, const char *new_password);

#endif // USER_H 