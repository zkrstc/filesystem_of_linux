#include "../include/inode.h"
#include "../include/disk.h"
#include "../include/ext2.h"
#include "../include/user.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

// Inode操作
int create_inode(uint16_t mode, uint16_t uid, uint16_t gid)
{
    uint32_t inode_no = allocate_inode();//返回空闲inode号（刚分配的）
    if (inode_no == 0)
    {
        return -1;
    }

    ext2_inode_t inode;
    /*
        memset()	C 标准库函数，用于填充内存块
        &inode	目标内存地址（inode 变量的地址）
        0	填充的值（每个字节都会被设为 0）
        sizeof(ext2_inode_t)	填充的字节数（ext2_inode_t 结构体的大小）
    */
    memset(&inode, 0, sizeof(ext2_inode_t));

    inode.i_mode = mode;
    inode.i_uid = uid;
    inode.i_gid = gid;
    inode.i_size = 0;
    inode.i_links_count = 1;
    inode.i_blocks = 0;
    inode.i_atime = time(NULL);
    inode.i_ctime = time(NULL);
    inode.i_mtime = time(NULL);

    // 初始化块指针数组
    for (int i = 0; i < 15; i++)
    {
        inode.i_block[i] = 0;
    }

    if (write_inode(inode_no, &inode) != 0)
    {
        free_inode(inode_no);
        return -1;
    }

    return inode_no;
}

int delete_inode(uint32_t inode_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    // 释放所有数据块
    for (int i = 0; i < 12; i++)
    {
        if (inode.i_block[i] != 0)
        {
            free_block(inode.i_block[i]);
        }
    }

    // 释放间接块（简化实现，只处理一级间接）
    if (inode.i_block[12] != 0)
    {
        uint32_t indirect_blocks[BLOCK_SIZE / 4];
        if (read_block(inode.i_block[12], indirect_blocks) == 0)
        {
            for (int i = 0; i < BLOCK_SIZE / 4; i++)
            {
                if (indirect_blocks[i] != 0)
                {
                    free_block(indirect_blocks[i]);
                }
            }
        }
        free_block(inode.i_block[12]);
    }

    // 清除inode
    memset(&inode, 0, sizeof(ext2_inode_t));
    write_inode(inode_no, &inode);

    // 释放inode
    free_inode(inode_no);

    return 0;
}

int get_inode_block(uint32_t inode_no, uint32_t block_index, uint32_t *block_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    if (block_index < 12)
    {
        *block_no = inode.i_block[block_index];
        //printf("[DEBUG] get_inode_block: inode=%u, block_index=%u, block_no=%u\n", inode_no, block_index, *block_no);
        return 0;
    }
    else if (block_index < 12 + BLOCK_SIZE / 4) 
    /*为什么是除以 4？
     因为每个物理块号是 uint32_t 类型，固定占 4 字节

     例如：

     块号 0x12345678 存储为 78 56 34 12（小端序）

     一个 1024 字节的块可存储 256 个这样的 4 字节块号*/
    {
        // 一级间接块
        if (inode.i_block[12] == 0)
        {
            *block_no = 0;
            return 0;
        }

        uint32_t indirect_blocks[BLOCK_SIZE / 4];
        if (read_block(inode.i_block[12], indirect_blocks) != 0)
        {
            return -1;
        }

        *block_no = indirect_blocks[block_index - 12];
        return 0;
    }

    return -1; // 超出范围
}

int set_inode_block(uint32_t inode_no, uint32_t block_index, uint32_t block_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    printf("[DEBUG] set_inode_block: inode=%u, block_index=%u, block_no=%u\n", 
           inode_no, block_index, block_no);

    if (block_index < 12)
    {
        inode.i_block[block_index] = block_no;
        printf("[DEBUG] set_inode_block: setting i_block[%u] = %u\n", block_index, block_no);
    }
    else if (block_index < 12 + BLOCK_SIZE / 4)
    {
        // 一级间接块
        if (inode.i_block[12] == 0)
        {
            inode.i_block[12] = allocate_block();
            if (inode.i_block[12] == 0)
            {
                return -1;
            }
        }

        uint32_t indirect_blocks[BLOCK_SIZE / 4];
        if (read_block(inode.i_block[12], indirect_blocks) != 0)
        {
            memset(indirect_blocks, 0, BLOCK_SIZE);
        }

        indirect_blocks[block_index - 12] = block_no;
        write_block(inode.i_block[12], indirect_blocks);
    }
    else
    {
        return -1; // 超出范围
    }

    int result = write_inode(inode_no, &inode);
    printf("[DEBUG] set_inode_block: write_inode returned %d\n", result);
    return result;
}

// 文件读写操作
ssize_t read_inode_data(uint32_t inode_no, void *buffer, size_t size, off_t offset)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    printf("[DEBUG] read_inode_data: inode=%u, size=%u, offset=%ld, requested_size=%zu\n", 
           inode_no, inode.i_size, offset, size);

    if (offset >= inode.i_size)
    {
        printf("[DEBUG] read_inode_data: offset >= file size, returning 0\n");
        return 0;
    }

    size_t bytes_read = 0;
    size_t remaining = size;
    off_t current_offset = offset;

    while (remaining > 0 && current_offset < inode.i_size)
    {
        uint32_t block_index = current_offset / BLOCK_SIZE;
        uint32_t block_offset = current_offset % BLOCK_SIZE;
        uint32_t block_no;

        if (get_inode_block(inode_no, block_index, &block_no) != 0 || block_no == 0)
        {
            printf("[DEBUG] read_inode_data: failed to get block %u or block is 0\n", block_index);
            break;
        }

        uint8_t block_buffer[BLOCK_SIZE];
        if (read_block(block_no, block_buffer) != 0)
        {
            printf("[DEBUG] read_inode_data: failed to read block %u\n", block_no);
            break;
        }

        size_t bytes_in_block = BLOCK_SIZE - block_offset;
        if (bytes_in_block > remaining)
        {
            bytes_in_block = remaining;
        }
        if (current_offset + bytes_in_block > inode.i_size)
        {
            bytes_in_block = inode.i_size - current_offset;
        }

        memcpy((char *)buffer + bytes_read, block_buffer + block_offset, bytes_in_block);

        bytes_read += bytes_in_block;
        remaining -= bytes_in_block;
        current_offset += bytes_in_block;
    }

    printf("[DEBUG] read_inode_data: actually read %zu bytes\n", bytes_read);

    // 更新访问时间
    update_atime(inode_no);

    return bytes_read;
}

ssize_t write_inode_data(uint32_t inode_no, const void *buffer, size_t size, off_t offset)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    printf("[DEBUG] write_inode_data: inode=%u, current_size=%u, offset=%ld, size=%zu\n", 
           inode_no, inode.i_size, offset, size);

    size_t bytes_written = 0;
    size_t remaining = size;//remaining表示剩余要写入的字节数，一开始比如是2000字节的话
    off_t current_offset = offset;

    while (remaining > 0)
    {
        uint32_t block_index = current_offset / BLOCK_SIZE;//比如24字节，block_index=0
        uint32_t block_offset = current_offset % BLOCK_SIZE;//如果2000字节，那么这里就是1024字节的溢出的部分
        uint32_t block_no;
        if (get_inode_block(inode_no, block_index, &block_no) != 0)
        {
            break;
        }

        if (block_no == 0)
        {
            block_no = allocate_block();//找到空闲的块号
            if (block_no == 0)
            {
                break;
            }
            if (set_inode_block(inode_no, block_index, block_no) != 0)//如果是24字节，设置inode的i_block数组，这里设置了i_block[0]=block_no
            //这里是更新了比如说inode为4号的i_block数组，但是前面的inode为4的i_block数组没有更新，所以需要重新读取inode
            {
                free_block(block_no);
                break;
            }
            // 重新读取inode以获取最新的i_block数组
            if (read_inode(inode_no, &inode) != 0) {
                break;
            }
        }

        uint8_t block_buffer[BLOCK_SIZE];
        if (read_block(block_no, block_buffer) != 0)
        {
            break;
        }

        size_t bytes_in_block = BLOCK_SIZE - block_offset;//根据上面如果是2000字节的话,BLOCK_SIZE-block_offset的话就是改块的剩余空间
        if (bytes_in_block > remaining)//如果改块剩余空间大于剩余要写入的字节数，那么就写入剩余要写入的字节数
        {
            bytes_in_block = remaining;
        }

        //否则的话就写入改块的剩余空间
        memcpy(block_buffer + block_offset, (char *)buffer + bytes_written, bytes_in_block);

        if (write_block(block_no, block_buffer) != 0)
        {
            break;
        }

        bytes_written += bytes_in_block;
        remaining -= bytes_in_block;
        current_offset += bytes_in_block;
    }

    // 更新文件大小和时间戳
    if (current_offset > inode.i_size)
    {
        inode.i_size = current_offset;
        inode.i_blocks = (inode.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        printf("[DEBUG] write_inode_data: updating file size from %u to %u\n", 
               (uint32_t)(current_offset - bytes_written), inode.i_size);
        
        // 将更新后的inode写回磁盘
        if (write_inode(inode_no, &inode) != 0) {
            printf("[DEBUG] write_inode_data: failed to write updated inode\n");
        }
    }

    update_mtime(inode_no);
    update_ctime(inode_no);

    printf("[DEBUG] write_inode_data: wrote %zu bytes, new size=%u\n", bytes_written, inode.i_size);

    return bytes_written;
}

int truncate_inode(uint32_t inode_no, off_t length)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    if (length >= inode.i_size)
    {
        return 0; // 不需要截断
    }

    uint32_t new_blocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t old_blocks = (inode.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // 释放多余的块
    for (uint32_t i = new_blocks; i < old_blocks; i++)
    {
        uint32_t block_no;
        if (get_inode_block(inode_no, i, &block_no) == 0 && block_no != 0)
        {
            free_block(block_no);
            set_inode_block(inode_no, i, 0);
        }
    }

    inode.i_size = length;
    inode.i_blocks = new_blocks;

    update_mtime(inode_no);
    update_ctime(inode_no);

    return write_inode(inode_no, &inode);
}
/*检查当前用户是否有权限 (access) 访问指定的 inode (inode_no)。
返回 1（有权限）或 0（无权限）。*/
int check_permission(uint32_t inode_no, int access)
{



    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return 0;
    }
    uint16_t uid = get_current_uid();
    uint16_t gid = get_current_gid();

    // root用户拥有所有权限
    if (uid == 0)
    {
        return 1;
    }

    uint16_t mode = 0;
    uint16_t access_mask = 0;
    
    if (uid == inode.i_uid)
    {
        mode = (inode.i_mode >> 6) & 0x7;
        access_mask = (access >> 6) & 0x7;
    }
    else if (gid == inode.i_gid)
    {
        mode = (inode.i_mode >> 3) & 0x7;
        access_mask = (access >> 3) & 0x7;
    }
    else
    {
        mode = inode.i_mode & 0x7;
        access_mask = access & 0x7;
    }

    return (mode & access_mask) == access_mask;
}

int change_permission(uint32_t inode_no, uint16_t mode)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    inode.i_mode = (inode.i_mode & 0xF000) | (mode & 0x0FFF);
    update_ctime(inode_no);

    return write_inode(inode_no, &inode);
}

int change_owner(uint32_t inode_no, uint16_t uid, uint16_t gid)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    inode.i_uid = uid;
    inode.i_gid = gid;
    update_ctime(inode_no);

    return write_inode(inode_no, &inode);
}

// 时间戳更新
void update_atime(uint32_t inode_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) == 0)
    {
        inode.i_atime = time(NULL);
        write_inode(inode_no, &inode);
    }
}

void update_mtime(uint32_t inode_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) == 0)
    {
        inode.i_mtime = time(NULL);
        write_inode(inode_no, &inode);
    }
}

void update_ctime(uint32_t inode_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) == 0)
    {
        inode.i_ctime = time(NULL);
        write_inode(inode_no, &inode);
    }
}

// 链接计数
int increment_link_count(uint32_t inode_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    inode.i_links_count++;
    update_ctime(inode_no);

    return write_inode(inode_no, &inode);
}

int decrement_link_count(uint32_t inode_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return -1;
    }

    if (inode.i_links_count > 0)
    {
        inode.i_links_count--;
    }
    update_ctime(inode_no);

    return write_inode(inode_no, &inode);
}

// 工具函数
int is_directory(uint32_t inode_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        printf("[DEBUG] is_directory: failed to read inode %u\n", inode_no);
        return 0;
    }
    int result = (inode.i_mode & 0xF000) == EXT2_S_IFDIR;
    printf("[DEBUG] is_directory: inode %u, mode=0x%x, is_dir=%d\n", inode_no, inode.i_mode, result);
    return result;
}

int is_regular_file(uint32_t inode_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return 0;
    }
    return (inode.i_mode & 0xF000) == EXT2_S_IFREG;
}

uint32_t get_file_size(uint32_t inode_no)
{
    ext2_inode_t inode;
    if (read_inode(inode_no, &inode) != 0)
    {
        return 0;
    }
    return inode.i_size;
}