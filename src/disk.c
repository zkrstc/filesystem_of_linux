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
void set_bitmap_bit(uint8_t *bitmap, int bit)
{
    int byte = bit / 8;
    int offset = bit % 8;
    bitmap[byte] |= (1 << offset);
}

void clear_bitmap_bit(uint8_t *bitmap, int bit)
{
    int byte = bit / 8;
    int offset = bit % 8;
    bitmap[byte] &= ~(1 << offset);
}

int get_bitmap_bit(uint8_t *bitmap, int bit)
{
    int byte = bit / 8;
    int offset = bit % 8;
    return (bitmap[byte] >> offset) & 1;
}

/*find_free_bit：

扫描 block_bitmap（块位图），寻找第一个 0（空闲块）。

如果找不到空闲块（所有位都是 1），返回 -1，此时 allocate_block 返回 0 表示分配失败。*/
int find_free_bit(uint8_t *bitmap, int size)
{
    for (int i = 0; i < size * 8; i++)
    {
        if (!get_bitmap_bit(bitmap, i))
        {
            return i;
        }
    }
    return -1;
}

/*block_no：要读取的块号（从 0 开始编号）。

buffer：目标内存缓冲区，用于存储读取的数据*/
int read_block(uint32_t block_no, void *buffer)
{
    if (disk_fd == -1)
    {
        return -1;
    }

    off_t offset = (off_t)block_no * BLOCK_SIZE;
    if (lseek(disk_fd, offset, SEEK_SET) == -1)
    {
        return -1;
    }

    ssize_t bytes_read = read(disk_fd, buffer, BLOCK_SIZE);
    if (bytes_read != BLOCK_SIZE)
    {
        return -1;
    }

    return 0;
}
/*每个块的大小为 BLOCK_SIZE（例如 4KB）。

块号 block_no 乘以 BLOCK_SIZE，得到该块在文件中的字节偏移量。

调用 lseek 将文件指针移动到 offset 处。

SEEK_SET 表示从文件开头计算偏移量。

如果移动失败（如 offset 超出文件大小），返回错误。

*/
int write_block(uint32_t block_no, const void *buffer)
{
    if (disk_fd == -1)
    {
        return -1;
    }

    off_t offset = (off_t)block_no * BLOCK_SIZE;
    if (lseek(disk_fd, offset, SEEK_SET) == -1)
    {
        return -1;
    }
    /*调用 write 将 buffer 中的 BLOCK_SIZE 字节数据写入当前文件指针位置。
    如果实际写入的字节数 bytes_written 不等于 BLOCK_SIZE，说明写入失败（可能磁盘已满或发生 I/O 错误），返回错误。*/
    ssize_t bytes_written = write(disk_fd, buffer, BLOCK_SIZE);
    if (bytes_written != BLOCK_SIZE)
    {
        return -1;
    }

    return 0;
}
/* (BLOCK_SIZE / sizeof(ext2_inode_t)得到的是多少inode占据一个块，比如1024/256=4也就是4个inode一个块*/
int read_inode(uint32_t inode_no, ext2_inode_t *inode)
{
    if (inode_no == 0 || inode_no >= MAX_INODES)
    {
        return -1;
    }

    // 计算inode在磁盘上的位置
    // 超级块占1个块，块位图占1个块，inode位图占1个块
    // inode表从第4个块开始
    // inode表存储的是inode信息，每个inode占用sizeof(ext2_inode_t)字节。
    uint32_t block_no = 3 + (inode_no - 1) / (BLOCK_SIZE / sizeof(ext2_inode_t));
    uint32_t offset = (inode_no - 1) % (BLOCK_SIZE / sizeof(ext2_inode_t));

    uint8_t buffer[BLOCK_SIZE];
    if (read_block(block_no, buffer) != 0)
    {
        return -1;
    }
    /*buffer + offset * sizeof(ext2_inode_t) 计算 inode 在缓冲区中的起始地址。
        memcpy 将数据拷贝到 inode 结构体。

    首先需要知道他的块号block_no，读到对应的块到buffer中，
    然后计算出inode在该块中的偏移量offset(块为单位的偏移，第一块，第二块，第三块)，最后将该块中对应的inode数据（大小为ext2_inode_t）拷贝到inode返回的指针中。
    */
    memcpy(inode, buffer + offset * sizeof(ext2_inode_t), sizeof(ext2_inode_t));
    return 0;
}

int write_inode(uint32_t inode_no, const ext2_inode_t *inode)
{
    //  inode_no：要写入的 inode 编号（从 1 开始编号）。
    //  inode：源内存结构体指针，存储待写入的 inode 数据。
    if (inode_no == 0 || inode_no >= MAX_INODES)
    {
        return -1;
    }

    uint32_t block_no = 3 + (inode_no - 1) / (BLOCK_SIZE / sizeof(ext2_inode_t));
    uint32_t offset = (inode_no - 1) % (BLOCK_SIZE / sizeof(ext2_inode_t));

    uint8_t buffer[BLOCK_SIZE];
    if (read_block(block_no, buffer) != 0)
    {
        return -1;
    }
    // 注意这里的buffer是块的起始地址，如果要写入的是inode_no=2的话，根据块偏移找到对应的位置（同上）
    memcpy(buffer + offset * sizeof(ext2_inode_t), inode, sizeof(ext2_inode_t));
    return write_block(block_no, buffer);
}

// 块分配和释放
uint32_t allocate_block(void)
{

    int free_bit = find_free_bit(block_bitmap, BLOCK_SIZE); // 返回分配的块号（从 1 开始）。
    if (free_bit == -1)
    {
        return 0; // 没有空闲块
    }

    set_bitmap_bit(block_bitmap, free_bit); // 设置块位图中的对应位为已分配
    fs.superblock.s_free_blocks_count--;

    // 写回位图
    write_block(1, block_bitmap);

    return free_bit + 1; // 块号从1开始
}

void free_block(uint32_t block_no)
{
    if (block_no == 0 || block_no > MAX_BLOCKS)
    {
        return;
    }

    clear_bitmap_bit(block_bitmap, block_no - 1);
    fs.superblock.s_free_blocks_count++;

    // 写回位图
    write_block(1, block_bitmap);
}

uint32_t allocate_inode(void)
{
    int free_bit = find_free_bit(inode_bitmap, BLOCK_SIZE);
    if (free_bit == -1)
    {
        return 0; // 没有空闲inode
    }

    set_bitmap_bit(inode_bitmap, free_bit);
    fs.superblock.s_free_inodes_count--;

    // 写回位图
    write_block(2, inode_bitmap);

    return free_bit + 1; // inode号从1开始
}

void free_inode(uint32_t inode_no)
{
    if (inode_no == 0 || inode_no > MAX_INODES)
    {
        return;
    }

    clear_bitmap_bit(inode_bitmap, inode_no - 1);
    fs.superblock.s_free_inodes_count++;

    // 写回位图
    write_block(2, inode_bitmap);
}

// 文件系统初始化
int init_disk_image(const char *filename)
{
    disk_fd = open(filename, O_RDWR);
    if (disk_fd == -1)
    {
        return -1;
    }

    // 读取位图
    if (read_block(1, block_bitmap) != 0)
    {
        close(disk_fd);
        disk_fd = -1;
        return -1;
    }

    if (read_block(2, inode_bitmap) != 0)
    {
        close(disk_fd);
        disk_fd = -1;
        return -1;
    }

    return 0;
}

void close_disk_image(void)
{
    if (disk_fd != -1)
    {
        close(disk_fd);
        disk_fd = -1;
    }
}