#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "ext2.h"

// 目录操作
int create_directory(const char *path, uint16_t mode);
int delete_directory(const char *path);
int list_directory(const char *path);
int change_directory(const char *path);

// 目录项操作
int add_directory_entry(uint32_t parent_inode, const char *name, uint32_t child_inode, uint8_t file_type);
int remove_directory_entry(uint32_t parent_inode, const char *name);
int find_directory_entry(uint32_t parent_inode, const char *name, ext2_dir_entry_t *entry);

// 路径解析
int path_to_inode(const char *path, uint32_t *inode_no);
int get_parent_inode(const char *path, uint32_t *parent_inode, char *child_name);

// 目录遍历
int read_directory_entries(uint32_t inode_no, ext2_dir_entry_t *entries, int max_entries);

// 特殊目录项
int create_dot_entries(uint32_t dir_inode, uint32_t parent_inode);

// 工具函数
int is_valid_filename(const char *name);
int normalize_path(char *path);

#endif // DIRECTORY_H 