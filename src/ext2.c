#include "../include/ext2.h"
#include "../include/disk.h"
#include "../include/user.h"
#include "../include/commands.h"
#include "../include/inode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 全局变量
ext2_fs_t fs;

// 文件系统初始化
int ext2_init(const char *disk_image) {
    // 初始化文件系统状态
    memset(&fs, 0, sizeof(ext2_fs_t));
    fs.current_user = -1;
    fs.next_fd = 3; // 0, 1, 2 是标准输入输出

    // 初始化用户系统（会自动从磁盘加载）
    init_users();

    // 挂载磁盘镜像
    if (disk_image != NULL) {
        if (init_disk_image(disk_image) != 0) {
            printf("Error: Failed to open disk image\n");
            return -1;
        }
        // 读取超级块
        if (read_block(0, &fs.superblock) != 0) {
            printf("Error: Failed to read superblock\n");
            close_disk_image();
            return -1;
        }
        // 校验魔数
        if (fs.superblock.s_magic != 0xEF53) {
            printf("Error: Invalid file system magic number\n");
            close_disk_image();
            return -1;
        }
        // 加载用户信息
        load_users_from_disk();
    }

    return 0;
}

// 文件系统格式化
int ext2_format(const char *disk_image) {
    printf("Formatting EXT2 file system: %s\n", disk_image);
    
    // 创建磁盘镜像文件
    FILE *fp = fopen(disk_image, "wb");
    if (fp == NULL) {
        printf("Error: Cannot create disk image file\n");
        return -1;
    }
    
    // 写入空块
    uint8_t zero_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (fwrite(zero_block, 1, BLOCK_SIZE, fp) != BLOCK_SIZE) {
            printf("Error: Failed to write block %d\n", i);
            fclose(fp);
            /*uint8_t zero_block[BLOCK_SIZE] = {0};
            fwrite(zero_block, 1, BLOCK_SIZE, fp);
            解释一下，fwrite使用fp作为文件指针，向文件中写入：起始地址为zero_block，写入长度为BLOCK_SIZE字节的内容。（1表示每个数据项为一字节）
            */
            return -1;
        }
    }
    
    fclose(fp);
    
    // 初始化超级块
    ext2_superblock_t superblock;
    memset(&superblock, 0, sizeof(superblock));
    
    // 设置超级块字段
    superblock.s_inodes_count = MAX_INODES;
    superblock.s_blocks_count = MAX_BLOCKS;
    superblock.s_r_blocks_count = 10; // 保留块数
    superblock.s_free_blocks_count = MAX_BLOCKS - 10;
    superblock.s_free_inodes_count = MAX_INODES - 1;
    superblock.s_first_data_block = 1;
    superblock.s_log_block_size = 0; // 1KB块
    superblock.s_log_frag_size = 0;
    superblock.s_blocks_per_group = MAX_BLOCKS;
    superblock.s_frags_per_group = MAX_BLOCKS;
    superblock.s_inodes_per_group = MAX_INODES;
    superblock.s_mtime = time(NULL);
    superblock.s_wtime = time(NULL);
    superblock.s_mnt_count = 0;
    superblock.s_max_mnt_count = 20;
    superblock.s_magic = 0xEF53; // EXT2魔数
    superblock.s_state = 1; // 干净状态
    superblock.s_errors = 1; // 继续
    superblock.s_minor_rev_level = 0;
    superblock.s_lastcheck = time(NULL);
    superblock.s_checkinterval = 1800; // 30分钟
    superblock.s_creator_os = 0; // Linux
    superblock.s_rev_level = 0;
    superblock.s_def_resuid = 0;
    superblock.s_def_resgid = 0;
    superblock.s_first_ino = 11;
    superblock.s_inode_size = sizeof(ext2_inode_t);
    superblock.s_block_group_nr = 0;
    superblock.s_feature_compat = 0;
    superblock.s_feature_incompat = 0;
    superblock.s_feature_ro_compat = 0;
    
    // 生成UUID
    for (int i = 0; i < 16; i++) {
        superblock.s_uuid[i] = rand() % 256;
    }
    
    strcpy(superblock.s_volume_name, "EXT2FS");
    strcpy(superblock.s_last_mounted, "/");
    
    // 写入超级块
    FILE *fp2 = fopen(disk_image, "r+b");
    if (fp2 == NULL) {
        printf("Error: Cannot write superblock\n");
        return -1;
    }
    
    if (fwrite(&superblock, 1, sizeof(superblock), fp2) != sizeof(superblock)) {
        printf("Error: Failed to write superblock\n");
        fclose(fp2);
        return -1;
    }
    
    fclose(fp2);
    
    // 初始化位图
    memset(block_bitmap, 0, BLOCK_SIZE);
    memset(inode_bitmap, 0, BLOCK_SIZE);
    
    // 标记已使用的块
    for (int i = 0; i < 10; i++) {
        set_bitmap_bit(block_bitmap, i);
    }
    
    // 标记已使用的inode
    set_bitmap_bit(inode_bitmap, 0); // inode 0 不使用
    
    // 写入位图
    FILE *fp3 = fopen(disk_image, "r+b");
    if (fp3 == NULL) {
        printf("Error: Cannot write bitmaps\n");
        return -1;
    }
    
    // 写入块位图
    fseek(fp3, BLOCK_SIZE, SEEK_SET);
    fwrite(block_bitmap, 1, BLOCK_SIZE, fp3);
    
    // 写入inode位图
    fseek(fp3, 2 * BLOCK_SIZE, SEEK_SET);
    fwrite(inode_bitmap, 1, BLOCK_SIZE, fp3);
    
    fclose(fp3);
    
    // 创建根目录
    if (init_disk_image(disk_image) != 0) {
        printf("Error: Failed to initialize disk image\n");
        return -1;
    }
    
    // 创建根目录inode
    uint32_t root_inode = create_inode(EXT2_S_IFDIR | 0755, 0, 0);
    printf("DEBUG: root_inode = %u\n", root_inode);
    if (root_inode == 0) {
        printf("Error: Failed to create root directory inode\n");
        close_disk_image();
        return -1;
    }
    
    // 分配根目录数据块
    uint32_t root_block = allocate_block();
    if (root_block == 0) {
        printf("Error: Failed to allocate root directory block\n");
        delete_inode(root_inode);
        close_disk_image();
        return -1;
    }
    
    // 设置根目录数据块
    set_inode_block(root_inode, 0, root_block);
    
    // 创建根目录的 . 和 .. 目录项
    uint8_t root_data[BLOCK_SIZE] = {0};
    ext2_dir_entry_t *entries = (ext2_dir_entry_t*)root_data;
    
    // . 目录项
    entries[0].inode = root_inode;
    entries[0].rec_len = sizeof(ext2_dir_entry_t);
    entries[0].name_len = 1;
    entries[0].file_type = 2; // 目录
    strcpy(entries[0].name, ".");
    
    // .. 目录项
    entries[1].inode = root_inode;
    entries[1].rec_len = sizeof(ext2_dir_entry_t);
    entries[1].name_len = 2;
    entries[1].file_type = 2; // 目录
    strcpy(entries[1].name, "..");
    
    // 写入根目录数据
    write_block(root_block, root_data);
    
    // 创建根目录后，初始化默认用户并保存到磁盘
    init_users();
    save_users_to_disk();
    
    close_disk_image();
    
    printf("EXT2 file system formatted successfully\n");
    return 0;
}

// 文件系统清理
void ext2_cleanup(void) {
    close_disk_image();
    printf("EXT2 file system cleaned up\n");
} 