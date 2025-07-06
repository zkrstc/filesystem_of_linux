#!/bin/bash

echo "=== Testing File Ownership and Permissions ==="

Available commands:
  format <disk_image>     - Format a new disk image
  mount <disk_image>      - Mount a disk image
  umount                  - Unmount current disk image
  status                  - Show file system status
  login <user> <pass>     - Login as user
  logout                  - Logout current user
  users                   - List all users
  mkdir <path>            - Create directory
  rmdir <path>            - Remove directory
  dir <path>              - List directory contents
  cd <path>               - Change directory
  create <path>           - Create file
  delete <path>           - Delete file
  open <path> <flags>     - Open file (0=read, 1=write, 2=readwrite)
  close <fd>              - Close file
  read <fd> <size>        - Read from file
  write <fd> <data>       - Write to file
  lseek <fd> <offset> <whence> - Move file pointer
  chmod <path> <mode>     - Change file permissions (root only)
  chown <path> <uid> <gid> - Change file owner (root only)
  useradd <user> <pass> <uid> <gid> - Add new user (root only)
  help                    - Show this help
  quit                    - Exit program


# 创建测试输入文件
cat > test_ownership_permissions.txt << 'EOF'
format disk.img
mount disk.img
status
login root root
mkdir /test
create /test/file1
chmod /test/file1 0700
dir
open file1 2
write 3 "This is a  file1 by root"
close 3
logout
login user1 user1#user1看不到根目录下的文件但是可以看到


cd /test
dir
open file1 0 #由于前面改了0700，所以user1不能读
cd ..
cd /home/user1
dir

create file1
dir
open file1 2
write 4 "This is a file1 by user1"

logout
login user2 user2
cd /home/user1
dir
open file1 0
read 5 100#输出user1的文件
close 5

cd /home/user2
create file
open file 2
write 6 "This is a file by user2"
close 6



logout 
login root root
dir

chown /test/file1 2 2#现在这个文件属于user2
chmod /test/file1 0777#现在这个文件的权限为777
logout
login user2 user2
open file1 2#没问题
read 7 100#显示内容为by root
close 7

create /test/file2 #不允许的，因为test目录还是属于root，权限为0700，其他用户不能访问

dir /test
logout
login user1 user1
# 测试：user1 尝试以写方式打开 root 创建的文件（应该被拒绝）
open /test/file1 1
# 测试：user1 尝试以只读方式打开 root 创建的文件（应该被拒绝，因为路径访问被拒绝）
open /test/file2 0
# 测试：user1 在自己的家目录中创建文件
create myfile.txt
dir
logout
quit
EOF

echo "Running ownership and permission test..."
./ext2fs < test_ownership_permissions.txt

echo "=== Test completed ===" 