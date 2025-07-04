#ifndef USER_H
#define USER_H

#include "ext2.h"

// 用户管理
void init_users(void);
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
int check_path_permission(const char *path, int access);
int check_user_path_access(const char *path, int access);

// 当前用户信息
uint16_t get_current_uid(void);
uint16_t get_current_gid(void);
const char* get_current_username(void);

// 用户列表
void list_users(void);

// 密码管理
int change_password(const char *username, const char *old_password, const char *new_password);

// 当前工作目录 inode 号
uint32_t get_cwd_inode();
void set_cwd_inode(uint32_t ino);

// 获取根目录 inode 号
uint32_t get_root_inode();

void save_users_to_disk(void);
void load_users_from_disk(void);

#endif // USER_H 