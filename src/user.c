#include "../include/user.h"
#include "../include/ext2.h"
#include "../include/disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypt.h>

// 全局变量
extern ext2_fs_t fs;

// 当前工作目录 inode 号
static uint32_t current_working_directory_inode = EXT2_ROOT_INO;

#define USER_BLOCK_NO 4 // 用户信息存储在第4号块

// 保存用户信息到磁盘
void save_users_to_disk() {
    write_block(USER_BLOCK_NO, fs.users);
}

// 从磁盘加载用户信息
void load_users_from_disk() {
    read_block(USER_BLOCK_NO, fs.users);
}

uint32_t get_cwd_inode() {
    return current_working_directory_inode;
}

void set_cwd_inode(uint32_t ino) {
    current_working_directory_inode = ino;
}

// 用户管理
void init_users() {
    // 先尝试从磁盘加载
    load_users_from_disk();
    // 检查 root 用户是否存在，不存在则初始化默认用户
    int has_root = 0;
    for (int i = 0; i < MAX_USERS; i++) {
        if (fs.users[i].is_active && strcmp(fs.users[i].username, "root") == 0) {
            has_root = 1;
            break;
        }
    }
    if (!has_root) {
        memset(fs.users, 0, sizeof(fs.users));
        strcpy(fs.users[0].username, "root");
        strcpy(fs.users[0].password, "root");
        fs.users[0].uid = 0;
        fs.users[0].gid = 0;
        fs.users[0].is_active = 1;
        strcpy(fs.users[1].username, "user1");
        strcpy(fs.users[1].password, "user1");
        fs.users[1].uid = 1;
        fs.users[1].gid = 1;
        fs.users[1].is_active = 1;
        save_users_to_disk();
    }
}

int add_user(const char *username, const char *password, uint16_t uid, uint16_t gid) {
    if (!username || !password) return -3; // 参数无效

    // 检查UID/GID冲突
    for (int i = 0; i < MAX_USERS; i++) {
        if (fs.users[i].is_active && 
           (fs.users[i].uid == uid || fs.users[i].gid == gid)) {
            return -2; // UID/GID冲突
        }
    }
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (!fs.users[i].is_active) {
            strncpy(fs.users[i].username, username, sizeof(fs.users[i].username) - 1);
            fs.users[i].username[sizeof(fs.users[i].username) - 1] = '\0';
            
            // 简单的密码加密（实际应用中应使用更安全的方法）
            strncpy(fs.users[i].password, password, sizeof(fs.users[i].password) - 1);
            fs.users[i].password[sizeof(fs.users[i].password) - 1] = '\0';
            
            fs.users[i].uid = uid;
            fs.users[i].gid = gid;
            fs.users[i].is_active = 1;
            save_users_to_disk();
            return 0;//找到就返回
        }
    }
    
    return -1; // 用户表已满
}

int remove_user(const char *username) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (fs.users[i].is_active && strcmp(fs.users[i].username, username) == 0) {
            fs.users[i].is_active = 0;
            save_users_to_disk();
            return 0;
        }
    }
    
    return -1; // 用户不存在
}

int find_user(const char *username) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (fs.users[i].is_active && strcmp(fs.users[i].username, username) == 0) {
            return i;
        }
    }
    
    return -1; // 用户不存在
}

// 用户认证
int login(const char *username, const char *password) {
    int user_index = find_user(username);
    if (user_index == -1) {
        return -1; // 用户不存在
    }
    
    // 简单的密码验证（实际应用中应使用加密）
    if (strcmp(fs.users[user_index].password, password) == 0) {
        fs.current_user = user_index;
        set_cwd_inode(EXT2_ROOT_INO); // 登录后初始化当前目录为根目录
        printf("Login successful. Welcome, %s!\n", username);
        return 0;
    }
    
    return -1; // 密码错误
}

void logout(void) {
    if (fs.current_user != -1) {
        printf("Logout successful. Goodbye, %s!\n", fs.users[fs.current_user].username);
        fs.current_user = -1;
        save_users_to_disk();
    }
}

int is_logged_in(void) {
    return fs.current_user != -1;
}

// 权限检查
int check_file_permission(uint32_t inode_no, int access) {
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0) {
        return 0;
    }
    
    uint16_t uid = get_current_uid();
    uint16_t gid = get_current_gid();
    
    // root用户有所有权限
    if (uid == 0) {
        return 1;
    }
    
    uint16_t mode = 0;
    if (uid == inode.i_uid) {
        mode = (inode.i_mode >> 6) & 0x7;
    } else if (gid == inode.i_gid) {
        mode = (inode.i_mode >> 3) & 0x7;
    } else {
        mode = inode.i_mode & 0x7;
    }
    
    return (mode & access) == access;
}

int check_directory_permission(uint32_t inode_no, int access) {
    return check_file_permission(inode_no, access);
}

// 当前用户信息
uint16_t get_current_uid(void) {
    if (fs.current_user == -1) {
        return 65535; // 无效用户ID
    }
    return fs.users[fs.current_user].uid;
}

uint16_t get_current_gid(void) {
    if (fs.current_user == -1) {
        return 65535; // 无效组ID
    }
    return fs.users[fs.current_user].gid;
}

const char* get_current_username(void) {
    if (fs.current_user == -1) {
        return "anonymous";
    }
    return fs.users[fs.current_user].username;
}

// 用户列表
void list_users(void) {
    printf("User List:\n");
    printf("%-15s %-10s %-10s %-10s\n", "Username", "UID", "GID", "Status");
    printf("----------------------------------------\n");
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (fs.users[i].is_active) {
            const char *status = (i == fs.current_user) ? "Logged in" : "Active";
            printf("%-15s %-10u %-10u %-10s\n", 
                   fs.users[i].username, 
                   fs.users[i].uid, 
                   fs.users[i].gid, 
                   status);
        }
    }
}

// 密码管理
int change_password(const char *username, const char *old_password, const char *new_password) {
    int user_index = find_user(username);
    if (user_index == -1) {
        return -1; // 用户不存在
    }
    
    // 验证旧密码
    if (strcmp(fs.users[user_index].password, old_password) != 0) {
        return -1; // 旧密码错误
    }
    
    // 更新密码
    strncpy(fs.users[user_index].password, new_password, sizeof(fs.users[user_index].password) - 1);
    fs.users[user_index].password[sizeof(fs.users[user_index].password) - 1] = '\0';
    
    return 0;
}

uint32_t get_root_inode() {
    return EXT2_ROOT_INO;
} 