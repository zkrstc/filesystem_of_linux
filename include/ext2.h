#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <time.h>
#include <sys/types.h>

// 文件系统常量
#define BLOCK_SIZE 1024
#define MAX_BLOCKS 1024
#define MAX_INODES 128
#define MAX_USERS 16
#define MAX_FILENAME 255
#define MAX_PATH 1024
#define MAX_OPEN_FILES 16
#define EXT2_ROOT_INO 2

// 文件类型
#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK  0xA000
#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFBLK  0x6000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFCHR  0x2000
#define EXT2_S_IFIFO  0x1000

// 权限位
#define EXT2_S_IRUSR 0x0100
#define EXT2_S_IWUSR 0x0080
#define EXT2_S_IXUSR 0x0040
#define EXT2_S_IRGRP 0x0020
#define EXT2_S_IWGRP 0x0010
#define EXT2_S_IXGRP 0x0008
#define EXT2_S_IROTH 0x0004
#define EXT2_S_IWOTH 0x0002
#define EXT2_S_IXOTH 0x0001

// 超级块结构
typedef struct {
    uint32_t s_inodes_count;      // Inode数量
    uint32_t s_blocks_count;      // 块数量
    uint32_t s_r_blocks_count;    // 保留块数量
    uint32_t s_free_blocks_count; // 空闲块数量
    uint32_t s_free_inodes_count; // 空闲inode数量
    uint32_t s_first_data_block;  // 第一个数据块
    uint32_t s_log_block_size;    // 块大小
    uint32_t s_log_frag_size;     // 片段大小
    uint32_t s_blocks_per_group;  // 每组块数
    uint32_t s_frags_per_group;   // 每组片段数
    uint32_t s_inodes_per_group;  // 每组inode数
    uint32_t s_mtime;             // 挂载时间
    uint32_t s_wtime;             // 写入时间
    uint16_t s_mnt_count;         // 挂载次数
    uint16_t s_max_mnt_count;     // 最大挂载次数
    uint16_t s_magic;             // 魔数
    uint16_t s_state;             // 文件系统状态
    uint16_t s_errors;            // 错误处理
    uint16_t s_minor_rev_level;   // 次版本号
    uint32_t s_lastcheck;         // 最后检查时间
    uint32_t s_checkinterval;     // 检查间隔
    uint32_t s_creator_os;        // 创建操作系统
    uint32_t s_rev_level;         // 版本号
    uint16_t s_def_resuid;        // 默认保留用户ID
    uint16_t s_def_resgid;        // 默认保留组ID
    uint32_t s_first_ino;         // 第一个非保留inode
    uint16_t s_inode_size;        // inode大小
    uint16_t s_block_group_nr;    // 块组号
    uint32_t s_feature_compat;    // 兼容特性
    uint32_t s_feature_incompat;  // 不兼容特性
    uint32_t s_feature_ro_compat; // 只读兼容特性
    uint8_t s_uuid[16];           // 文件系统UUID
    char s_volume_name[16];       // 卷名
    char s_last_mounted[64];      // 最后挂载点
    uint32_t s_journal_uuid[4];   // 日志UUID
} ext2_superblock_t;

// Inode结构
typedef struct {
    uint16_t i_mode;              // 文件类型和访问权限
    uint16_t i_uid;               // 用户ID
    uint32_t i_size;              // 文件大小（字节）
    uint32_t i_atime;             // 最后访问时间
    uint32_t i_ctime;             // 创建时间
    uint32_t i_mtime;             // 最后修改时间
    uint32_t i_dtime;             // 删除时间
    uint16_t i_gid;               // 组ID
    uint16_t i_links_count;       // 硬链接数
    uint32_t i_blocks;            // 块数
    uint32_t i_flags;             // 文件标志
    uint32_t i_block[15];         // 块指针数组
    uint32_t i_generation;        // 文件版本
    uint32_t i_file_acl;          // 文件ACL
    uint32_t i_dir_acl;           // 目录ACL
    uint32_t i_faddr;             // 片段地址
    uint8_t i_frag;               // 片段号
    uint8_t i_fsize;              // 片段大小
    uint16_t i_pad1;              // 填充
    uint32_t i_reserved2[2];      // 保留
} ext2_inode_t;

// 目录项结构
typedef struct {
    uint32_t inode;               // inode号
    uint16_t rec_len;             // 目录项长度
    uint8_t name_len;             // 名称长度
    uint8_t file_type;            // 文件类型
    char name[255];               // 文件名
} ext2_dir_entry_t;

// 用户结构
typedef struct {
    char username[32];
    char password[32];
    uint16_t uid;
    uint16_t gid;
    int is_active;
} user_t;

// 打开文件结构
typedef struct {
    int fd;
    uint32_t inode_no;
    int flags;
    off_t offset;
    int is_open;
} open_file_t;

// 文件系统状态
typedef struct {
    ext2_superblock_t superblock;
    user_t users[MAX_USERS];
    int current_user;
    open_file_t open_files[MAX_OPEN_FILES];
    int next_fd;
    char disk_image[256];
} ext2_fs_t;

// 函数声明
int ext2_init(const char *disk_image);
int ext2_format(const char *disk_image);
void ext2_cleanup(void);

// 全局变量
extern ext2_fs_t fs;

#endif // EXT2_H 