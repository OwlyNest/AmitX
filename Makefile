# Toolchain
CC := i686-elf-gcc
LD := i686-elf-ld

# Directories
SRC_DIRS := src \
	shell \
	fs \
	boot \
	arch/x86 \
	drivers \
	lib \
	mm \
	kernel \
	screen \
	apps \
	tests \
	logo \
	ui \
	hw

BUILD_DIR := build

# Output
TARGET := kernel.bin

# Flags
CFLAGS := \
	-m32 \
	-ffreestanding \
	-O2 \
	-Wall \
	-Wextra \
	-Werror \
	-MMD \
	-MP \
	-Iinclude \

CFLAGS += -DAMITX_BUILD_DATE="\"$(shell date +%Y-%m-%d)\""

LDFLAGS := \
	-T boot/linker.ld \
	-nostdlib

# --------------------------------------------------------------------
# Source discovery
# --------------------------------------------------------------------

C_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
S_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.S))

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
OBJS += $(patsubst %.S,$(BUILD_DIR)/%.o,$(S_SRCS))

DEPS := $(OBJS:.o=.d)

# --------------------------------------------------------------------
# Targets
# --------------------------------------------------------------------

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# --------------------------------------------------------------------
# C compilation
# --------------------------------------------------------------------

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# --------------------------------------------------------------------
# Assembly compilation
# --------------------------------------------------------------------

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# --------------------------------------------------------------------
# Cleanup
# --------------------------------------------------------------------

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# --------------------------------------------------------------------
# Auto-generated dependencies
# --------------------------------------------------------------------

-include $(DEPS)