
# Tools
CC = i686-elf-gcc
LD = i686-elf-ld

# Flags
CFLAGS  = -m32 -ffreestanding -O2 -Wall -Wextra -Iinclude -IAmitC -Icyclone -I..
LDFLAGS = -T boot/linker.ld -nostdlib

# Directories
SRC_DIR = src
CYCLONE_DIR = cyclone
BOOT_DIR = boot

# Sources and objects
SRC_C = $(wildcard $(SRC_DIR)/*.c)
SRC_CYCLONE = $(wildcard $(CYCLONE_DIR)/*.c)
SRC_S = $(wildcard $(BOOT_DIR)/*.S) $(wildcard $(SRC_DIR)/*.S)
OBJS  = $(SRC_C:.c=.o) $(SRC_S:.S=.o) $(SRC_CYCLONE:.c=.o)

# Default target
all: kernel.bin

# Compile C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly files
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@


# Link kernel
kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# Clean
clean:
	rm -f $(OBJS) kernel.bin
