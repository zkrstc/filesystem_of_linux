#ifndef INODE_H
#define INODE_H

#include "ext2.h"
#include <sys/types.h>

// Inode操作
int create_inode(uint16_t mode, uint16_t uid, uint16_t gid);
int delete_inode(uint32_t inode_no);
int get_inode_block(uint32_t inode_no, uint32_t block_index, uint32_t *block_no);
int set_inode_block(uint32_t inode_no, uint32_t block_index, uint32_t block_no);

// 文件读写操作
ssize_t read_inode_data(uint32_t inode_no, void *buffer, size_t size, off_t offset);
ssize_t write_inode_data(uint32_t inode_no, const void *buffer, size_t size, off_t offset);
int truncate_inode(uint32_t inode_no, off_t length);

// 权限检查
int check_permission(uint32_t inode_no, int access);
int change_permission(uint32_t inode_no, uint16_t mode);
int change_owner(uint32_t inode_no, uint16_t uid, uint16_t gid);

// 时间戳更新
void update_atime(uint32_t inode_no);
void update_mtime(uint32_t inode_no);
void update_ctime(uint32_t inode_no);

// 链接计数
int increment_link_count(uint32_t inode_no);
int decrement_link_count(uint32_t inode_no);

// 工具函数
int is_directory(uint32_t inode_no);
int is_regular_file(uint32_t inode_no);
uint32_t get_file_size(uint32_t inode_no);

#endif // INODE_H 