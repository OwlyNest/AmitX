# --------------------------------------------------------------------
# Toolchain
# --------------------------------------------------------------------

CC       := i686-elf-gcc
CXX      := i686-elf-g++
LD       := $(CC)
AS       := $(CC)

AR       := i686-elf-ar
NM       := i686-elf-nm
SIZE     := i686-elf-size
OBJDUMP  := i686-elf-objdump
OBJCOPY  := i686-elf-objcopy
READELF  := i686-elf-readelf

RUSTC    ?= rustc
CARGO    ?= cargo

# --------------------------------------------------------------------
# Project layout
# --------------------------------------------------------------------

TARGET     := kernel.elf
BUILD_DIR  := build

SRC_DIRS := \
    src \
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

ACPICA_DIR := third_party/acpica/components
ACPICA_INC := third_party/acpica/include

COMMON_FLAGS := \
    -m32 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-asynchronous-unwind-tables \
    -fno-unwind-tables \
    -fno-pic \
    -fno-pie \
    -O2 \
    -MMD \
    -MP \
    -D__OWLYNEST__ \
    -Iinclude

CFLAGS := \
    $(COMMON_CFLAGS) \
    -Wall \
    -Wextra \
    -Werror \
    -isystem $(ACPICA_INC)

ACPICA_CFLAGS := \
    $(COMMON_CFLAGS) \
    -Wall \
    -Wno-unused-parameter \
    -Wno-sign-compare \
    -I$(ACPICA_INC)

CXXFLAGS := \
    $(COMMON_FLAGS) \
    -Wall \
    -Wextra \
    -Werror \
    -fno-exceptions \
    -fno-rtti \
    -fno-threadsafe-statics \
    -fno-use-cxa-atexit \
    -std=c++20

RUSTFLAGS := \
    -C panic=abort \
    -C opt-level=2 \
    -C relocation-model=static \
    -C force-frame-pointers=no

LDFLAGS := \
    -T boot/linker.ld \
    -nostdlib

LIBS := -lgcc

C_SRCS    := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c))
CPP_SRCS  := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.cpp))
ASM_SRCS  := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.S))
RUST_SRCS := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.rs))
ACPICA_SRCS := $(shell find $(ACPICA_DIR) -name '*.c')

C_OBJS    := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
CPP_OBJS  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CPP_SRCS))
ASM_OBJS  := $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))
RUST_OBJS := $(patsubst %.rs,$(BUILD_DIR)/%.o,$(RUST_SRCS))

OBJS := \
    $(C_OBJS) \
    $(CPP_OBJS) \
    $(ASM_OBJS) \
    $(ACPICA_OBJS) \
    $(RUST_OBJS)

DEPS := $(OBJS:.o=.d)

define compile-c
	@mkdir -p $(dir $@)
	$(CC) $(1) -c $< -o $@
endef

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ACPICA_OBJS): $(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(ACPICA_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_FLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.rs
	@mkdir -p $(dir $@)
	$(RUSTC) \
		--crate-type lib \
		--emit=obj \
		-C panic=abort \
		-C opt-level=2 \
		-C relocation-model=static \
		-o $@ \
		$<

.PHONY: \
    clean \
    size \
    sections \
    symbols \
    readelf \
    acpica-only

size:
	$(SIZE) $(TARGET)

sections:
	$(OBJDUMP) -h $(TARGET)

symbols:
	$(NM) -n $(TARGET)

readelf:
	$(READELF) -l $(TARGET)

