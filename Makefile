# Toolchain
CC := i686-elf-gcc
CXX := i686-elf-g++
LD := $(CC)

BUILD_DIR := build

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
	exec \
	sync \
	acpi

# --------------------------------------------------------------------
# ACPICA (vendored third-party — recursive glob, relaxed warnings)
# --------------------------------------------------------------------
# --------------------------------------------------------------------
# ACPICA (explicit module selection)
# --------------------------------------------------------------------

# --------------------------------------------------------------------
# ACPICA (vendored third-party)
# --------------------------------------------------------------------
ACPICA_DIR := third_party/acpica/components
ACPICA_INC := third_party/acpica/include

ACPICA_CORE_SRCS := $(shell find $(ACPICA_DIR) -name '*.c')
ACPICA_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(ACPICA_CORE_SRCS))

ACPICA_CFLAGS := \
    -m32 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-asynchronous-unwind-tables \
    -fno-unwind-tables \
    -fno-pic \
    -fno-pie \
    -O2 \
    -Wall \
    -Wno-unused-parameter \
    -Wno-sign-compare \
    -D__OWLYNEST__ \
    -I$(ACPICA_INC) \
    -Iinclude

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

CFLAGS += -D__OWLYNEST__ -isystem third_party/acpica/include

LDFLAGS := \
	-T boot/linker.ld \
	-nostdlib

LIBS := -lgcc

# --------------------------------------------------------------------
# Source discovery
# --------------------------------------------------------------------

C_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
C_SRCS := $(filter-out arch/x86/cpuid.c,$(C_SRCS))
S_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.S))

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
OBJS += $(patsubst %.S,$(BUILD_DIR)/%.o,$(S_SRCS))
OBJS += $(ACPICA_OBJS)

DEPS := $(OBJS:.o=.d)
DEPS += $(ACPICA_OBJS:.o=.d)

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

$(ACPICA_OBJS): $(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(ACPICA_CFLAGS) -MMD -MP -c $< -o $@

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

.PHONY: acpica-only
acpica-only: $(ACPICA_OBJS)
	@echo "[x] ACPICA compiled cleanly: $(words $(ACPICA_OBJS)) objects"