# Toolchain
CC := i686-elf-gcc
LD := $(CC)

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
	hw \
	gfx \
	exec

BUILD_DIR := build

# Output
TARGET := kernel.elf

# Flags
CFLAGS := \
    -m32 \
    -ffreestanding \
    -fno-stack-protector \
	-fno-asynchronous-unwind-tables \
	-fno-unwind-tables \
    -fno-pic \
    -fno-pie \
    -O2 \
    -Wall \
    -Wextra \
    -Werror \
    -MMD \
    -MP \
    -Iinclude

CFLAGS += -DAMITX_BUILD_DATE="\"$(shell date +%Y-%m-%d)\""

LDFLAGS := \
	-T boot/linker.ld \
	-nostdlib

LIBS := -lgcc

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
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

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

.PHONY: size sections symbols

size: kernel.elf
	i686-elf-size kernel.elf

sections: kernel.elf
	i686-elf-objdump -h kernel.elf

symbols: kernel.elf
	i686-elf-nm -n kernel.elf | less

readelf: kernel.elf
	i686-elf-readelf -l kernel.elf