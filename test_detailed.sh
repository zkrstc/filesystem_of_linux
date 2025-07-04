#!/bin/bash

echo "=== 详细测试多级目录结构和权限管理 ==="
echo

# 停止后台进程
pkill -f ext2fs

# 格式化磁盘
echo "1. 格式化磁盘..."
echo "format disk.img" | timeout 5s ./ext2fs

# 挂载磁盘
echo "2. 挂载磁盘..."
echo "mount disk.img" | timeout 5s ./ext2fs

echo "3. 测试root用户登录和家目录..."
echo "login root root" | timeout 5s ./ext2fs

echo "4. 测试root用户权限（应该能访问所有目录）..."
echo "mkdir /home" | timeout 5s ./ext2fs
echo "mkdir /home/user1" | timeout 5s ./ext2fs
echo "mkdir /home/user2" | timeout 5s ./ext2fs
echo "create /home/user1/test.txt" | timeout 5s ./ext2fs
echo "open /home/user1/test.txt 2" | timeout 5s ./ext2fs
echo "write 1 'Hello from root'" | timeout 5s ./ext2fs
echo "close 1" | timeout 5s ./ext2fs

echo "5. 测试user1用户登录和家目录..."
echo "logout" | timeout 5s ./ext2fs
echo "login user1 user1" | timeout 5s ./ext2fs

echo "6. 测试user1用户权限（应该只能访问/home/user1）..."
echo "mkdir /home/user1/mydir" | timeout 5s ./ext2fs
echo "create /home/user1/myfile.txt" | timeout 5s ./ext2fs
echo "open /home/user1/myfile.txt 2" | timeout 5s ./ext2fs
echo "write 1 'Hello from user1'" | timeout 5s ./ext2fs
echo "close 1" | timeout 5s ./ext2fs

echo "7. 测试user1无法访问其他目录..."
echo "mkdir /home/user2/test" | timeout 5s ./ext2fs
echo "dir /home/user2" | timeout 5s ./ext2fs
echo "cd /root" | timeout 5s ./ext2fs

echo "8. 测试user1无法执行特权命令..."
echo "chmod /home/user1/myfile.txt 0644" | timeout 5s ./ext2fs
echo "useradd user3 password3 3 3" | timeout 5s ./ext2fs

echo "9. 切换回root测试特权命令..."
echo "logout" | timeout 5s ./ext2fs
echo "login root root" | timeout 5s ./ext2fs
echo "chmod /home/user1/myfile.txt 0644" | timeout 5s ./ext2fs
echo "useradd user3 password3 3 3" | timeout 5s ./ext2fs

echo "10. 测试多级目录路径显示..."
echo "cd /home/user1" | timeout 5s ./ext2fs
echo "status" | timeout 5s ./ext2fs

echo "=== 测试完成 ===" 