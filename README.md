# EXT2 File System Simulator

这是一个模拟Linux EXT2文件系统的实现，用于学习和理解文件系统的工作原理。

## 功能特性

### 核心功能
- **文件系统格式化**: 创建新的EXT2文件系统镜像
- **文件操作**: 创建、删除、打开、关闭、读取、写入文件
- **目录操作**: 创建、删除、列出目录内容、切换目录
- **用户管理**: 用户登录、注销、权限管理
- **权限控制**: 文件读写权限、所有者管理

### 技术特性
- **Inode管理**: 完整的inode结构，支持12个直接块和1个间接块
- **块分配**: 位图管理的空闲块分配
- **目录结构**: 支持多级目录结构
- **权限系统**: 用户、组、其他用户的读写执行权限
- **时间戳**: 文件的创建、修改、访问时间

## 编译和运行

### 编译
```bash
make
```

### 运行
```bash
./ext2fs
```

### 清理
```bash
make clean
```

## 使用说明

### 1. 格式化文件系统
```
format disk.img
```
创建一个新的EXT2文件系统镜像文件。

### 2. 挂载文件系统
```
mount disk.img
```
挂载指定的文件系统镜像。

### 3. 用户登录
```
login root root
```
使用用户名和密码登录系统。

### 4. 目录操作
```
mkdir /home
mkdir /home/user1
cd /home/user1
dir /
```
创建目录、切换目录、列出目录内容。

### 5. 文件操作
```
create /test.txt
open /test.txt 2
write 3 "Hello, World!"
read 3 20
close 3
```
创建文件、打开文件、写入数据、读取数据、关闭文件。

### 6. 权限管理
```
chmod /test.txt 644
chown /test.txt 1 1
```
修改文件权限和所有者。

### 7. 系统状态
```
status
users
```
查看文件系统状态和用户列表。

## 命令参考

### 文件系统管理
- `format <disk_image>` - 格式化新的磁盘镜像
- `mount <disk_image>` - 挂载磁盘镜像
- `umount` - 卸载当前磁盘镜像
- `status` - 显示文件系统状态

### 用户管理
- `login <username> <password>` - 用户登录
- `logout` - 用户注销
- `users` - 列出所有用户

### 目录操作
- `mkdir <path>` - 创建目录
- `rmdir <path>` - 删除目录
- `dir <path>` - 列出目录内容
- `cd <path>` - 切换目录

### 文件操作
- `create <path>` - 创建文件
- `delete <path>` - 删除文件
- `open <path> <flags>` - 打开文件 (0=读, 1=写, 2=读写)
- `close <fd>` - 关闭文件
- `read <fd> <size>` - 从文件读取数据
- `write <fd> <data>` - 向文件写入数据

### 权限管理
- `chmod <path> <mode>` - 修改文件权限 (八进制)
- `chown <path> <uid> <gid>` - 修改文件所有者

### 其他
- `help` - 显示帮助信息
- `quit` - 退出程序

## 文件系统结构

### 磁盘布局
```
Block 0:    Superblock
Block 1:    Block Bitmap
Block 2:    Inode Bitmap
Block 3-4:  Inode Table
Block 5+:   Data Blocks
```

### Inode结构
- 文件类型和权限
- 用户ID和组ID
- 文件大小
- 时间戳 (创建、修改、访问时间)
- 块指针数组 (12个直接块 + 1个间接块)

### 目录项结构
- inode号
- 记录长度
- 名称长度
- 文件类型
- 文件名

## 默认用户

系统预置了以下用户：
- `root/root` (UID: 0, GID: 0)
- `user1/password1` (UID: 1, GID: 1)
- `user2/password2` (UID: 2, GID: 1)

## 示例会话

```
ext2fs> format test.img
Formatting disk image: test.img
EXT2 file system formatted successfully

ext2fs> mount test.img
Disk image mounted: test.img

ext2fs> login root root
Login successful. Welcome, root!

ext2fs> mkdir /home
Directory created: /home

ext2fs> mkdir /home/user1
Directory created: /home/user1

ext2fs> create /home/user1/test.txt
File created: /home/user1/test.txt

ext2fs> open /home/user1/test.txt 2
File opened: /home/user1/test.txt (fd=3)

ext2fs> write 3 "Hello, EXT2 File System!"
File written successfully

ext2fs> close 3
File closed: fd=3

ext2fs> dir /home/user1
Directory listing for: /home/user1
Name                 Inode      Type       Size        Permissions
------------------------------------------------------------
.                    2          d          1024        drwxr-xr-x
..                   1          d          1024        drwxr-xr-x
test.txt             3          -          23          -rw-r--r--

ext2fs> status
File System Status:
Disk image: test.img
Total blocks: 1024
Free blocks: 1013
Total inodes: 128
Free inodes: 125
Current user: root
Open files: 0

ext2fs> quit
Goodbye!
```

## 技术细节

### 块大小
- 默认块大小: 1024字节
- 最大块数: 1024个
- 最大inode数: 128个

### 文件大小限制
- 直接块: 12个 (12KB)
- 间接块: 1个 (256KB)
- 最大文件大小: 268KB

### 权限位
- 用户权限: rwx (读、写、执行)
- 组权限: rwx
- 其他用户权限: rwx

## 注意事项

1. 这是一个教学用的简化实现，不支持所有EXT2特性
2. 文件系统镜像存储在二进制文件中
3. 不支持软链接、硬链接等高级特性
4. 密码存储未加密，仅用于演示
5. 不支持文件系统检查工具

## 开发环境

- 操作系统: Linux
- 编译器: GCC
- 标准: C99

## 许可证

本项目仅用于教育和学习目的。 