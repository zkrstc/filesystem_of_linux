#include "../include/user.h"
#include "../include/inode.h"
#include "../include/directory.h"
#include "../include/disk.h"
#include "../include/ext2.h"
#include "../include/commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// 目录操作
int create_directory(const char *path, uint16_t mode) {
    uint32_t parent_inode;
    char child_name[MAX_FILENAME];

    // 递归创建父目录
    if (get_parent_inode(path, &parent_inode, child_name) != 0) {
        char parent_path[MAX_PATH];
        strncpy(parent_path, path, sizeof(parent_path) - 1);
        parent_path[sizeof(parent_path) - 1] = '\0';
        char *last_slash = strrchr(parent_path, '/');
        if (last_slash && last_slash != parent_path) {
            *last_slash = '\0';
            // 递归调用 create_directory
            if (create_directory(parent_path, 0755) != 0) {
                printf("DEBUG: Failed to recursively create parent directory: %s\n", parent_path);
                return -1;
            }
        }
        // 再次获取父目录
        if (get_parent_inode(path, &parent_inode, child_name) != 0) {
            printf("DEBUG: Failed to get parent inode for path: %s\n", path);
            return -1;
        }
    }

    // 检查父目录是否为目录
    if (!is_directory(parent_inode)) {
        printf("DEBUG: Parent inode %u is not a directory\n", parent_inode);
        return -1;
    }
    // 检查权限
    if (!check_permission(parent_inode, EXT2_S_IWUSR)) {
        printf("DEBUG: Permission denied for parent inode %u\n", parent_inode);
        return -1;
    }
    // 检查是否已存在
    uint32_t exist_inode;
    if (find_child_inode(parent_inode, child_name, &exist_inode) == 0) {
        // 已存在
        return 0;
    }
    // 创建目录inode，权限严格按参数mode设置，owner为当前用户
    uint32_t dir_inode = create_inode(EXT2_S_IFDIR | (mode & 0777), get_current_uid(), get_current_gid());
    if (dir_inode == 0) {
        printf("DEBUG: Failed to create directory inode\n");
        return -1;
    }
    // 分配数据块
    uint32_t data_block = allocate_block();
    if (data_block == 0) {
        printf("DEBUG: Failed to allocate data block\n");
        delete_inode(dir_inode);
        return -1;
    }
    set_inode_block(dir_inode, 0, data_block);
    // 创建 . 和 .. 目录项
    if (create_dot_entries(dir_inode, parent_inode) != 0) {
        printf("DEBUG: Failed to create dot entries\n");
        delete_inode(dir_inode);
        return -1;
    }
    // 在父目录中添加目录项
    if (add_directory_entry(parent_inode, child_name, dir_inode, 2) != 0) {
        printf("DEBUG: Failed to add directory entry to parent\n");
        delete_inode(dir_inode);
        return -1;
    }
    return 0;
}

int delete_directory(const char *path) {
    uint32_t inode_no;
    if (path_to_inode(path, &inode_no) != 0) {
        return -1;
    }
    
    if (!is_directory(inode_no)) {
        return -1;
    }
    
    // 检查权限
    if (!check_permission(inode_no, EXT2_S_IWUSR)) {
        return -1;
    }
    
    // 检查目录是否为空（除了 . 和 ..）
    ext2_dir_entry_t entries[64];
    int count = read_directory_entries(inode_no, entries, 64);
    if (count > 2) {
        return -1; // 目录不为空
    }
    
    // 获取父目录
    uint32_t parent_inode;
    char child_name[MAX_FILENAME];
    if (get_parent_inode(path, &parent_inode, child_name) != 0) {
        return -1;
    }
    
    // 从父目录中删除目录项
    if (remove_directory_entry(parent_inode, child_name) != 0) {
        return -1;
    }
    
    // 删除目录inode
    return delete_inode(inode_no);
}

// 计算目录的总大小（递归计算所有子文件和子目录的大小）
uint32_t calculate_directory_size(uint32_t dir_inode) {
    uint32_t total_size = 0;
    ext2_dir_entry_t entries[64];
    int count = read_directory_entries(dir_inode, entries, 64);
    
    for (int i = 0; i < count; i++) {
        if (entries[i].inode == 0) continue;
        
        // 跳过 . 和 .. 目录项
        if (strcmp(entries[i].name, ".") == 0 || strcmp(entries[i].name, "..") == 0) {
            continue;
        }
        
        ext2_inode_t inode;
        if (read_inode(entries[i].inode, &inode) != 0) continue;
        
        if (is_directory(entries[i].inode)) {
            // 递归计算子目录的大小
            total_size += calculate_directory_size(entries[i].inode);
        } else {
            // 普通文件，直接加上文件大小
            total_size += inode.i_size;
        }
    }
    
    return total_size;
}

int list_directory(const char *path) {
    uint32_t inode_no;
    if (path_to_inode(path, &inode_no) != 0) {
        printf("Error: Directory not found: %s\n", path);
        return -1;
    }
    if (!is_directory(inode_no)) {
        printf("Error: Not a directory: %s\n", path);
        return -1;
    }
    // 检查权限
    if (!check_permission(inode_no, EXT2_S_IRUSR)) {
        printf("Error: Permission denied\n");
        return -1;
    }
    ext2_dir_entry_t entries[64];
    int count = read_directory_entries(inode_no, entries, 64);
    printf("Directory listing for: %s\n", path);
    printf("%-20s %-10s %-10s %-10s %-10s %-10s %-10s\n", "Name", "Inode", "Type", "Size", "Permissions", "Owner", "Address");
    printf("--------------------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        if (entries[i].inode == 0) continue;
        ext2_inode_t inode;
        if (read_inode(entries[i].inode, &inode) != 0) continue;
        char type_char = '?';
        if (is_directory(entries[i].inode)) type_char = 'd';
        else if (is_regular_file(entries[i].inode)) type_char = '-';
        
        // 计算显示的大小
        uint32_t display_size = inode.i_size;
        if (is_directory(entries[i].inode)) {
            // 对于目录，计算其下所有文件的总大小
            display_size = calculate_directory_size(entries[i].inode);
        }
        
        char permissions[11];
        snprintf(permissions, sizeof(permissions), "%c%c%c%c%c%c%c%c%c%c",
                type_char,
                (inode.i_mode & EXT2_S_IRUSR) ? 'r' : '-',
                (inode.i_mode & EXT2_S_IWUSR) ? 'w' : '-',
                (inode.i_mode & EXT2_S_IXUSR) ? 'x' : '-',
                (inode.i_mode & EXT2_S_IRGRP) ? 'r' : '-',
                (inode.i_mode & EXT2_S_IWGRP) ? 'w' : '-',
                (inode.i_mode & EXT2_S_IXGRP) ? 'x' : '-',
                (inode.i_mode & EXT2_S_IROTH) ? 'r' : '-',
                (inode.i_mode & EXT2_S_IWOTH) ? 'w' : '-',
                (inode.i_mode & EXT2_S_IXOTH) ? 'x' : '-');
        
        // 获取所有者用户名
        char owner_name[32] = "unknown";
        for (int j = 0; j < MAX_USERS; j++) {
            if (fs.users[j].is_active && fs.users[j].uid == inode.i_uid) {
                strncpy(owner_name, fs.users[j].username, sizeof(owner_name) - 1);
                owner_name[sizeof(owner_name) - 1] = '\0';
                break;
            }
        }
        
        printf("%-20s %-10u %-10c %-10u %-10s %-10s %-10u\n",
               entries[i].name, entries[i].inode, type_char, display_size, permissions, owner_name, entries[i].inode);
    }
    return 0;
}

int change_directory(const char *path) {
    uint32_t inode_no;
    char full_path[MAX_PATH];
    if (path[0] != '/') {
        // 相对路径，拼接当前目录
        char cwd[MAX_PATH];
        get_cwd_path(cwd, sizeof(cwd));
        if (strcmp(cwd, "/") == 0) {
            // /user1
            strncpy(full_path, "/", sizeof(full_path) - 1);
            full_path[sizeof(full_path) - 1] = '\0';
            strncat(full_path, path, sizeof(full_path) - strlen(full_path) - 1);
        } else {
            // /home/user1
            strncpy(full_path, cwd, sizeof(full_path) - 1);
            full_path[sizeof(full_path) - 1] = '\0';
            strncat(full_path, "/", sizeof(full_path) - strlen(full_path) - 1);
            strncat(full_path, path, sizeof(full_path) - strlen(full_path) - 1);
        }
    } else {
        strncpy(full_path, path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }
    printf("Changing directory to: %s\n", full_path);
    if (path_to_inode(full_path, &inode_no) != 0) {
        printf("Error: Path does not exist\n");
        return -1;
    }
    if (!is_directory(inode_no)) {
        printf("Error: Path is not a directory\n");
        return -1;
    }
    // 检查权限
    if (!check_permission(inode_no, EXT2_S_IXUSR)) {
        printf("Error: No execute permission on directory\n");
        return -1;
    }
    set_cwd_inode(inode_no);
    printf("Successfully changed to directory with inode: %u\n", inode_no);
    return 0;
}

// 目录项操作
int add_directory_entry(uint32_t parent_inode, const char *name, uint32_t child_inode, uint8_t file_type) {
    ext2_inode_t parent;
    if (read_inode(parent_inode, &parent) != 0) {
        return -1;
    }
    
    // 查找空闲空间
    uint32_t block_index = 0;
    uint32_t block_no;
    uint8_t buffer[BLOCK_SIZE];
    
    while (block_index < 12) {
        if (get_inode_block(parent_inode, block_index, &block_no) != 0) {
            break;
        }
        
        if (block_no == 0) {
            // 分配新块
            block_no = allocate_block();
            if (block_no == 0) {
                return -1;
            }
            set_inode_block(parent_inode, block_index, block_no);
            memset(buffer, 0, BLOCK_SIZE);
        } else {
            if (read_block(block_no, buffer) != 0) {
                return -1;
            }
        }
        
        // 查找空闲目录项
        ext2_dir_entry_t *entry = (ext2_dir_entry_t*)buffer;
        int entry_count = BLOCK_SIZE / sizeof(ext2_dir_entry_t);
        
        for (int i = 0; i < entry_count; i++) {
            if (entry[i].inode == 0) {
                // 找到空闲项
                entry[i].inode = child_inode;
                entry[i].rec_len = sizeof(ext2_dir_entry_t);
                entry[i].name_len = strlen(name);
                entry[i].file_type = file_type;
                strncpy(entry[i].name, name, sizeof(entry[i].name) - 1);
                entry[i].name[sizeof(entry[i].name) - 1] = '\0';
                
                write_block(block_no, buffer);
                increment_link_count(child_inode);
                return 0;
            }
        }
        
        block_index++;
    }
    
    return -1; // 没有空间
}

int remove_directory_entry(uint32_t parent_inode, const char *name) {
    ext2_inode_t parent;
    if (read_inode(parent_inode, &parent) != 0) {
        return -1;
    }
    
    uint32_t block_index = 0;
    uint32_t block_no;
    uint8_t buffer[BLOCK_SIZE];
    
    while (block_index < 12) {
        if (get_inode_block(parent_inode, block_index, &block_no) != 0 || block_no == 0) {
            break;
        }
        
        if (read_block(block_no, buffer) != 0) {
            return -1;
        }
        
        ext2_dir_entry_t *entry = (ext2_dir_entry_t*)buffer;
        int entry_count = BLOCK_SIZE / sizeof(ext2_dir_entry_t);
        
        for (int i = 0; i < entry_count; i++) {
            if (entry[i].inode != 0 && strcmp(entry[i].name, name) == 0) {
                uint32_t child_inode = entry[i].inode;
                entry[i].inode = 0; // 标记为删除
                
                write_block(block_no, buffer);
                decrement_link_count(child_inode);
                return 0;
            }
        }
        
        block_index++;
    }
    
    return -1; // 未找到
}

int find_directory_entry(uint32_t parent_inode, const char *name, ext2_dir_entry_t *entry) {
    ext2_inode_t parent;
    if (read_inode(parent_inode, &parent) != 0) {
        return -1;
    }
    
    uint32_t block_index = 0;
    uint32_t block_no;
    uint8_t buffer[BLOCK_SIZE];
    
    while (block_index < 12) {
        if (get_inode_block(parent_inode, block_index, &block_no) != 0 || block_no == 0) {
            break;
        }
        
        if (read_block(block_no, buffer) != 0) {
            return -1;
        }
        
        ext2_dir_entry_t *entries = (ext2_dir_entry_t*)buffer;
        int entry_count = BLOCK_SIZE / sizeof(ext2_dir_entry_t);
        
        for (int i = 0; i < entry_count; i++) {
            if (entries[i].inode != 0 && strcmp(entries[i].name, name) == 0) {
                memcpy(entry, &entries[i], sizeof(ext2_dir_entry_t));
                return 0;
            }
        }
        
        block_index++;
    }
    
    return -1; // 未找到
}

// 路径解析
int path_to_inode(const char *path, uint32_t *inode_no) {
    if (strcmp(path, "/") == 0) {
        *inode_no = EXT2_ROOT_INO; // 根目录
        return 0;
    }
    
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    char *token = strtok(path_copy, "/");
    uint32_t current_inode;
    
    // 处理相对路径和绝对路径
    if (path[0] == '/') {
        // 绝对路径，从根目录开始
        current_inode = EXT2_ROOT_INO;
    } else {
        // 相对路径，从当前目录开始
        current_inode = get_cwd_inode();
    }
    
    while (token != NULL) {
        ext2_dir_entry_t entry;
        if (find_directory_entry(current_inode, token, &entry) != 0) {
            return -1;
        }
        
        current_inode = entry.inode;
        token = strtok(NULL, "/");
    }
    
    *inode_no = current_inode;
    return 0;
}

int get_parent_inode(const char *path, uint32_t *parent_inode, char *child_name) {
    printf("DEBUG: get_parent_inode called with path: %s\n", path);
    
    if (strcmp(path, "/") == 0) {
        printf("DEBUG: Path is root directory\n");
        *parent_inode = get_root_inode(); // 根目录
        printf("DEBUG: Parent is root directory, inode: %u\n", *parent_inode);
        return 0;
    }
    
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    char *last_slash = strrchr(path_copy, '/');
    if (last_slash == NULL) {
        // 相对路径
        printf("DEBUG: Relative path detected\n");
        strcpy(child_name, path_copy);
        *parent_inode = get_cwd_inode(); // 当前目录
        printf("DEBUG: Child name: %s, parent inode: %u\n", child_name, *parent_inode);
        return 0;
    }
    
    *last_slash = '\0';
    strcpy(child_name, last_slash + 1);
    
    printf("DEBUG: Path after removing last component: '%s', child name: %s\n", path_copy, child_name);
    
    if (strlen(path_copy) == 0) {
        *parent_inode = get_root_inode(); // 根目录
        printf("DEBUG: Parent is root directory, inode: %u\n", *parent_inode);
    } else {
        printf("DEBUG: Resolving parent path: %s\n", path_copy);
        int result = path_to_inode(path_copy, parent_inode);
        printf("DEBUG: path_to_inode result: %d, parent inode: %u\n", result, *parent_inode);
        return result;
    }
    
    return 0;
}

// 目录遍历
int read_directory_entries(uint32_t inode_no, ext2_dir_entry_t *entries, int max_entries) {
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0) {
        return -1;
    }
    
    int entry_count = 0;
    uint32_t block_index = 0;
    uint32_t block_no;
    uint8_t buffer[BLOCK_SIZE];
    
    while (block_index < 12 && entry_count < max_entries) {
        if (get_inode_block(inode_no, block_index, &block_no) != 0 || block_no == 0) {
            break;
        }
        
        if (read_block(block_no, buffer) != 0) {
            break;
        }
        
        ext2_dir_entry_t *block_entries = (ext2_dir_entry_t*)buffer;
        int block_entry_count = BLOCK_SIZE / sizeof(ext2_dir_entry_t);
        
        for (int i = 0; i < block_entry_count && entry_count < max_entries; i++) {
            if (block_entries[i].inode != 0) {
                memcpy(&entries[entry_count], &block_entries[i], sizeof(ext2_dir_entry_t));
                entry_count++;
            }
        }
        
        block_index++;
    }
    
    return entry_count;
}

// 特殊目录项
int create_dot_entries(uint32_t dir_inode, uint32_t parent_inode) {
    // 创建 . 目录项
    if (add_directory_entry(dir_inode, ".", dir_inode, 2) != 0) {
        return -1;
    }
    
    // 创建 .. 目录项
    if (add_directory_entry(dir_inode, "..", parent_inode, 2) != 0) {
        return -1;
    }
    
    return 0;
}

// 工具函数
int is_valid_filename(const char *name) {
    if (name == NULL || strlen(name) == 0 || strlen(name) > MAX_FILENAME) {
        return 0;
    }
    
    // 检查是否包含非法字符
    for (int i = 0; name[i] != '\0'; i++) {
        if (name[i] == '/' || name[i] == '\0') {
            return 0;
        }
    }
    
    return 1;
}

int normalize_path(char *path) {
    // 简化实现，移除多余的斜杠
    char *src = path;
    char *dst = path;
    
    while (*src) {
        if (*src == '/' && *(src + 1) == '/') {
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    
    return 0;
}

// 查找子目录的inode
int find_child_inode(uint32_t parent_inode, const char *name, uint32_t *child_inode) {
    ext2_dir_entry_t entry;
    if (find_directory_entry(parent_inode, name, &entry) == 0) {
        *child_inode = entry.inode;
        return 0;
    }
    return -1;
} 

// 递归创建多级目录