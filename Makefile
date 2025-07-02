CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = ext2fs
SOURCES = src/main.c src/ext2.c src/inode.c src/directory.c src/user.c src/disk.c src/commands.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = include/ext2.h include/inode.h include/directory.h include/user.h include/disk.h include/commands.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -f *.img

run: $(TARGET)
	./$(TARGET) 