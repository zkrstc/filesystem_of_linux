#!/bin/bash

echo "=== 测试多级目录结构和权限管理 ==="
echo

# 格式化磁盘
echo "1. 格式化磁盘..."
echo "format disk.img" | ./ext2fs > /dev/null 2>&1

# 挂载磁盘
echo "2. 挂载磁盘..."
echo "mount disk.img" | ./ext2fs > /dev/null 2>&1

# 测试root用户登录
echo "3. 测试root用户登录..."
echo "login root root" | ./ext2fs > /dev/null 2>&1

# 测试root用户权限
echo "4. 测试root用户权限..."
echo "mkdir /home" | ./ext2fs > /dev/null 2>&1
echo "mkdir /home/user1" | ./ext2fs > /dev/null 2>&1
echo "mkdir /home/user2" | ./ext2fs > /dev/null 2>&1
echo "create /home/user1/test.txt" | ./ext2fs > /dev/null 2>&1
echo "write 1 'Hello from root'" | ./ext2fs > /dev/null 2>&1
echo "close 1" | ./ext2fs > /dev/null 2>&1

# 测试user1用户登录
echo "5. 测试user1用户登录..."
echo "logout" | ./ext2fs > /dev/null 2>&1
echo "login user1 user1" | ./ext2fs > /dev/null 2>&1

# 测试user1用户权限
echo "6. 测试user1用户权限..."
echo "mkdir /home/user1/mydir" | ./ext2fs > /dev/null 2>&1
echo "create /home/user1/myfile.txt" | ./ext2fs > /dev/null 2>&1
echo "write 1 'Hello from user1'" | ./ext2fs > /dev/null 2>&1
echo "close 1" | ./ext2fs > /dev/null 2>&1

# 测试user1无法访问其他目录
echo "7. 测试user1无法访问其他目录..."
echo "mkdir /home/user2/test" | ./ext2fs > /dev/null 2>&1
echo "dir /home/user2" | ./ext2fs > /dev/null 2>&1
echo "cd /root" | ./ext2fs > /dev/null 2>&1

# 测试特权命令
echo "8. 测试特权命令..."
echo "chmod /home/user1/myfile.txt 0644" | ./ext2fs > /dev/null 2>&1
echo "useradd user3 password3 3 3" | ./ext2fs > /dev/null 2>&1

# 切换回root测试特权命令
echo "9. 切换回root测试特权命令..."
echo "logout" | ./ext2fs > /dev/null 2>&1
echo "login root root" | ./ext2fs > /dev/null 2>&1
echo "chmod /home/user1/myfile.txt 0644" | ./ext2fs > /dev/null 2>&1
echo "useradd user3 password3 3 3" | ./ext2fs > /dev/null 2>&1

echo "=== 测试完成 ===" 