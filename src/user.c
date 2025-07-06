#include "../include/user.h"
#include "../include/ext2.h"
#include "../include/disk.h"
#include "../include/directory.h"
#include "../include/inode.h"
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

        strcpy(fs.users[2].username, "user2");
        strcpy(fs.users[2].password, "user2");
        fs.users[2].uid = 2;
        fs.users[2].gid = 2;
        fs.users[2].is_active = 1;

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
        
        // 根据用户类型设置家目录
        if (strcmp(username, "root") == 0) {
            // root用户的家目录是 /root
            uint32_t root_home_inode;
            if (path_to_inode("/root", &root_home_inode) == 0) {
                set_cwd_inode(root_home_inode);
            } else {
                // 如果 /root 目录不存在，创建它（root用户有权限）
                if (create_directory("/root", 0700) == 0) {
                    path_to_inode("/root", &root_home_inode);
                    set_cwd_inode(root_home_inode);
                } else {
                    set_cwd_inode(EXT2_ROOT_INO); // 回退到根目录
                }
            }
        } else {
            // 普通用户的家目录是 /home/username
            char home_path[256];
            snprintf(home_path, sizeof(home_path), "/home/%s", username);
            uint32_t home_inode;
            if (path_to_inode(home_path, &home_inode) == 0) {
                set_cwd_inode(home_inode);
            } else {
                // 如果家目录不存在，临时切换为根用户创建
                int original_user = fs.current_user;
                fs.current_user = 0; // 临时切换为root用户
                
                // 先创建 /home 目录（如果不存在）
                uint32_t home_dir_inode;
                if (path_to_inode("/home", &home_dir_inode) != 0) {
                    if (create_directory("/home", 0755) != 0) {
                        printf("Warning: Failed to create /home directory\n");
                        fs.current_user = original_user;
                        set_cwd_inode(EXT2_ROOT_INO);
                        printf("Login successful. Welcome, %s!\n", username);
                        return 0;
                    }
                }
                
                // 创建用户家目录
                if (create_directory(home_path, 0755) == 0) {
                    path_to_inode(home_path, &home_inode);
                    set_cwd_inode(home_inode);
                    
                    // 将家目录的所有者改为新用户
                    if (change_owner(home_inode, fs.users[original_user].uid, fs.users[original_user].gid) == 0) {
                        printf("Home directory created and ownership set: %s\n", home_path);
                    } else {
                        printf("Warning: Failed to set home directory ownership\n");
                    }
                } else {
                    printf("Warning: Failed to create home directory\n");
                    set_cwd_inode(EXT2_ROOT_INO);
                }
                
                // 恢复原用户身份
                fs.current_user = original_user;
            }
        }
        
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
    uint16_t access_mask = 0;
    
    if (uid == inode.i_uid) {
        mode = (inode.i_mode >> 6) & 0x7;
        access_mask = (access >> 6) & 0x7;
    } else if (gid == inode.i_gid) {
        mode = (inode.i_mode >> 3) & 0x7;
        access_mask = (access >> 3) & 0x7;
    } else {
        mode = inode.i_mode & 0x7;
        access_mask = access & 0x7;
    }
    
    int result = (mode & access_mask) == access_mask;
    return result;
}

int check_directory_permission(uint32_t inode_no, int access) {
    return check_file_permission(inode_no, access);
}

// 检查路径权限（包括路径上的所有目录）
int check_path_permission(const char *path, int access) {
    uint16_t uid = get_current_uid();
    
    // root用户有所有权限
    if (uid == 0) {
        return 1;
    }
    
    // 解析路径，检查每一级目录的权限
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    // 从根目录开始
    uint32_t current_inode = EXT2_ROOT_INO;
    
    // 跳过开头的斜杠
    char *token = strtok(path_copy, "/");
    
    while (token != NULL) {
        // 检查当前目录的访问权限
        
        // 根据当前用户类型选择正确的执行权限位
        int execute_permission;
        ext2_inode_t inode;
        if (read_inode(current_inode, &inode) == 0) {
            if (uid == inode.i_uid) {
                execute_permission = EXT2_S_IXUSR;
            } else if (get_current_gid() == inode.i_gid) {
                execute_permission = EXT2_S_IXGRP;
            } else {
                execute_permission = EXT2_S_IXOTH;
            }
        } else {
            execute_permission = EXT2_S_IXOTH; // 默认使用其他用户权限
        }
        
        if (!check_directory_permission(current_inode, execute_permission)) {
            return 0; // 没有执行权限
        }
        
        // 查找下一级目录
        uint32_t child_inode;
        if (find_child_inode(current_inode, token, &child_inode) != 0) {
            // 如果找不到子目录，检查当前目录的写权限（用于创建）
            if (access & EXT2_S_IWUSR) {
                return check_directory_permission(current_inode, EXT2_S_IWUSR);
            }
            return 0;
        }
        
        current_inode = child_inode;
        token = strtok(NULL, "/");
    }
    
    // 检查最终目标的权限
    return check_file_permission(current_inode, access);
}

// 检查用户是否有权限访问特定路径
// 这个函数主要处理路径级别的访问控制，而不是文件级别的权限
int check_user_path_access(const char *path, int access) {
    uint16_t uid = get_current_uid();

    // root用户有所有权限
    if (uid == 0) {
        return 1;
    }

    // 获取当前用户名
    const char *username = get_current_username();
    
    // 普通用户只能访问自己的家目录及其下内容，以及/home目录本身（允许cd ..）
    if (strcmp(username, "root") != 0 && strcmp(username, "anonymous") != 0) {
        
        // 允许访问根目录 /，但只允许读和执行，不允许写
        if (strcmp(path, "/") == 0) {
            if ((access & EXT2_S_IWUSR) == 0) {
                int ret = check_path_permission(path, access);
                return ret;
            } else {
                return 0;
            }
        }
        
        // 允许访问根目录下的公共文件夹（如 /test），但只允许读和执行，不允许写
        if (path[0] == '/' && strchr(path + 1, '/') == NULL) {
            // 这是根目录下的直接子目录或文件
            if (strcmp(path, "/home") != 0 && strcmp(path, "/root") != 0) {
                // 不是 /home 或 /root，允许访问但只读
                if ((access & EXT2_S_IWUSR) == 0) {
                    int ret = check_path_permission(path, access);
                    return ret;
                } else {
                    return 0;
                }
            }
        }
        
        // 允许访问 /home 目录
        if (strcmp(path, "/home") == 0) {
            int ret = check_path_permission(path, access);
            return ret;
        }
        
        // 允许访问 /root 目录，只能读（不允许写/执行）
        if (strcmp(path, "/root") == 0) {
            if ((access & (EXT2_S_IWUSR | EXT2_S_IXUSR)) == 0) {
                int ret = check_path_permission(path, access);
                return ret;
            } else {
                return 0;
            }
        }
        
        // 允许访问自己的家目录及其子目录
        char home_path[256];
        snprintf(home_path, sizeof(home_path), "/home/%s", username);
        if (strncmp(path, home_path, strlen(home_path)) == 0) {
            // 允许访问 /home/username 或 /home/username/xxx
            if (path[strlen(home_path)] == '\0' || path[strlen(home_path)] == '/') {
                int ret = check_path_permission(path, access);
                return ret;
            }
        }
        
        // 允许访问其他用户的家目录，但只允许读和执行，不允许写
        if (strncmp(path, "/home/", 6) == 0) {
            const char *path_part = path + 6; // 跳过 "/home/"
            char *slash = strchr(path_part, '/');
            if (slash != NULL) {
                *slash = '\0'; // 临时截断，获取用户名
                char other_username[256];
                strncpy(other_username, path_part, sizeof(other_username) - 1);
                other_username[sizeof(other_username) - 1] = '\0';
                *slash = '/'; // 恢复路径
                
                // 如果不是自己的家目录，只允许读和执行
                if (strcmp(other_username, username) != 0) {
                    if ((access & EXT2_S_IWUSR) == 0) {
                        // 只允许读和执行，不允许写
                        int ret = check_path_permission(path, access);
                        return ret;
                    } else {
                        return 0;
                    }
                }
            } else {
                // 路径是 /home/username 格式
                if (strcmp(path_part, username) != 0) {
                    if ((access & EXT2_S_IWUSR) == 0) {
                        // 只允许读和执行，不允许写
                        int ret = check_path_permission(path, access);
                        return ret;
                    } else {
                        return 0;
                    }
                }
            }
        }
        
        // 允许访问 .. 路径（即 /home 目录）
        if (strcmp(path, "..") == 0) {
            int ret = check_path_permission("/home", access);
            return ret;
        }
        
        // 对于相对路径，需要检查当前目录的权限
        if (path[0] != '/' && strchr(path, '/') == NULL) {
            // 这是当前目录下的文件，需要检查当前目录的权限
            uint32_t cwd_inode = get_cwd_inode();
            
            // 检查当前目录的读权限（用于列出文件）或写权限（用于创建文件）
            int dir_access = (access & EXT2_S_IWUSR) ? EXT2_S_IWUSR : EXT2_S_IRUSR;
            int ret = check_directory_permission(cwd_inode, dir_access);
            return ret;
        }
        
        // 对于 "." 路径（当前目录），检查当前目录的权限
        if (strcmp(path, ".") == 0) {
            uint32_t cwd_inode = get_cwd_inode();
            
            // 检查当前目录的读权限（用于列出文件）或写权限（用于创建文件）
            int dir_access = (access & EXT2_S_IWUSR) ? EXT2_S_IWUSR : EXT2_S_IRUSR;
            int ret = check_directory_permission(cwd_inode, dir_access);
            return ret;
        }
        
        return 0; // 不能访问其他路径
    }

    return 0; // 其他用户无权限
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