#include "../include/disk.h"
#include "../include/ext2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// 全局变量
static int disk_fd = -1;
uint8_t block_bitmap[BLOCK_SIZE];
uint8_t inode_bitmap[BLOCK_SIZE];

// 位图操作
void set_bitmap_bit(uint8_t *bitmap, int bit) {
    int byte = bit / 8;
    int offset = bit % 8;
    bitmap[byte] |= (1 << offset);
}

void clear_bitmap_bit(uint8_t *bitmap, int bit) {
    int byte = bit / 8;
    int offset = bit % 8;
    bitmap[byte] &= ~(1 << offset);
}

int get_bitmap_bit(uint8_t *bitmap, int bit) {
    int byte = bit / 8;
    int offset = bit % 8;
    return (bitmap[byte] >> offset) & 1;
}

int find_free_bit(uint8_t *bitmap, int size) {
    for (int i = 0; i < size * 8; i++) {
        if (!get_bitmap_bit(bitmap, i)) {
            return i;
        }
    }
    return -1;
}

// 磁盘操作
int read_block(uint32_t block_no, void *buffer) {
    if (disk_fd == -1) {
        return -1;
    }
    
    off_t offset = (off_t)block_no * BLOCK_SIZE;
    if (lseek(disk_fd, offset, SEEK_SET) == -1) {
        return -1;
    }
    
    ssize_t bytes_read = read(disk_fd, buffer, BLOCK_SIZE);
    if (bytes_read != BLOCK_SIZE) {
        return -1;
    }
    
    return 0;
}

int write_block(uint32_t block_no, const void *buffer) {
    if (disk_fd == -1) {
        return -1;
    }
    
    off_t offset = (off_t)block_no * BLOCK_SIZE;
    if (lseek(disk_fd, offset, SEEK_SET) == -1) {
        return -1;
    }
    
    ssize_t bytes_written = write(disk_fd, buffer, BLOCK_SIZE);
    if (bytes_written != BLOCK_SIZE) {
        return -1;
    }
    
    return 0;
}

int read_inode(uint32_t inode_no, ext2_inode_t *inode) {
    if (inode_no == 0 || inode_no >= MAX_INODES) {
        return -1;
    }
    
    // 计算inode在磁盘上的位置
    // 超级块占1个块，块位图占1个块，inode位图占1个块
    // inode表从第4个块开始
    uint32_t block_no = 3 + (inode_no - 1) / (BLOCK_SIZE / sizeof(ext2_inode_t));
    uint32_t offset = (inode_no - 1) % (BLOCK_SIZE / sizeof(ext2_inode_t));
    
    uint8_t buffer[BLOCK_SIZE];
    if (read_block(block_no, buffer) != 0) {
        return -1;
    }
    
    memcpy(inode, buffer + offset * sizeof(ext2_inode_t), sizeof(ext2_inode_t));
    return 0;
}

int write_inode(uint32_t inode_no, const ext2_inode_t *inode) {
    if (inode_no == 0 || inode_no >= MAX_INODES) {
        return -1;
    }
    
    uint32_t block_no = 3 + (inode_no - 1) / (BLOCK_SIZE / sizeof(ext2_inode_t));
    uint32_t offset = (inode_no - 1) % (BLOCK_SIZE / sizeof(ext2_inode_t));
    
    uint8_t buffer[BLOCK_SIZE];
    if (read_block(block_no, buffer) != 0) {
        return -1;
    }
    
    memcpy(buffer + offset * sizeof(ext2_inode_t), inode, sizeof(ext2_inode_t));
    return write_block(block_no, buffer);
}

// 块分配和释放
uint32_t allocate_block(void) {
    int free_bit = find_free_bit(block_bitmap, BLOCK_SIZE);
    if (free_bit == -1) {
        return 0; // 没有空闲块
    }
    
    set_bitmap_bit(block_bitmap, free_bit);
    fs.superblock.s_free_blocks_count--;
    
    // 写回位图
    write_block(1, block_bitmap);
    
    return free_bit + 1; // 块号从1开始
}

void free_block(uint32_t block_no) {
    if (block_no == 0 || block_no > MAX_BLOCKS) {
        return;
    }
    
    clear_bitmap_bit(block_bitmap, block_no - 1);
    fs.superblock.s_free_blocks_count++;
    
    // 写回位图
    write_block(1, block_bitmap);
}

uint32_t allocate_inode(void) {
    int free_bit = find_free_bit(inode_bitmap, BLOCK_SIZE);
    if (free_bit == -1) {
        return 0; // 没有空闲inode
    }
    
    set_bitmap_bit(inode_bitmap, free_bit);
    fs.superblock.s_free_inodes_count--;
    
    // 写回位图
    write_block(2, inode_bitmap);
    
    return free_bit + 1; // inode号从1开始
}

void free_inode(uint32_t inode_no) {
    if (inode_no == 0 || inode_no > MAX_INODES) {
        return;
    }
    
    clear_bitmap_bit(inode_bitmap, inode_no - 1);
    fs.superblock.s_free_inodes_count++;
    
    // 写回位图
    write_block(2, inode_bitmap);
}

// 文件系统初始化
int init_disk_image(const char *filename) {
    disk_fd = open(filename, O_RDWR);
    if (disk_fd == -1) {
        return -1;
    }
    
    // 读取位图
    if (read_block(1, block_bitmap) != 0) {
        close(disk_fd);
        disk_fd = -1;
        return -1;
    }
    
    if (read_block(2, inode_bitmap) != 0) {
        close(disk_fd);
        disk_fd = -1;
        return -1;
    }
    
    return 0;
}

void close_disk_image(void) {
    if (disk_fd != -1) {
        close(disk_fd);
        disk_fd = -1;
    }
} 