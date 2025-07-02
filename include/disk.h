#ifndef DISK_H
#define DISK_H

#include "ext2.h"
#include <stdint.h>

// 位图操作
void set_bitmap_bit(uint8_t *bitmap, int bit);
void clear_bitmap_bit(uint8_t *bitmap, int bit);
int get_bitmap_bit(uint8_t *bitmap, int bit);
int find_free_bit(uint8_t *bitmap, int size);

// 磁盘操作
int read_block(uint32_t block_no, void *buffer);
int write_block(uint32_t block_no, const void *buffer);
int read_inode(uint32_t inode_no, ext2_inode_t *inode);
int write_inode(uint32_t inode_no, const ext2_inode_t *inode);

// 块分配和释放
uint32_t allocate_block(void);
void free_block(uint32_t block_no);
uint32_t allocate_inode(void);
void free_inode(uint32_t inode_no);

// 文件系统初始化
int init_disk_image(const char *filename);
void close_disk_image(void);

// 位图管理
extern uint8_t block_bitmap[BLOCK_SIZE];
extern uint8_t inode_bitmap[BLOCK_SIZE];

#endif // DISK_H 