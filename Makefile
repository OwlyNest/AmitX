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
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

SRC_DIRS := \
    shell \
    fs \
    boot \
    arch/x86 \
    drivers \
    lib \
    mm \
    kernel \
    screen \
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
    $(COMMON_FLAGS) \
    -Wall \
    -Wextra \
    -Werror \
    -isystem $(ACPICA_INC)

ACPICA_CFLAGS := \
    $(COMMON_FLAGS) \
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

rwildcard = $(foreach d,$(wildcard $1/*),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

C_SRCS    := $(foreach d,$(SRC_DIRS),$(call rwildcard,$(d),*.c))
CPP_SRCS  := $(foreach d,$(SRC_DIRS),$(call rwildcard,$(d),*.cpp))
ASM_SRCS  := $(foreach d,$(SRC_DIRS),$(call rwildcard,$(d),*.S))
RUST_SRCS := $(foreach d,$(SRC_DIRS),$(call rwildcard,$(d),*.rs))
ACPICA_SRCS := $(shell find $(ACPICA_DIR) -name '*.c')

C_OBJS    := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
CPP_OBJS  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CPP_SRCS))
ASM_OBJS  := $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))
ACPICA_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(ACPICA_SRCS))
$(ACPICA_OBJS): CFLAGS := $(ACPICA_CFLAGS)
RUST_OBJS := $(patsubst %.rs,$(BUILD_DIR)/%.o,$(RUST_SRCS))

OBJS := \
    $(C_OBJS) \
    $(CPP_OBJS) \
    $(ASM_OBJS) \
    $(ACPICA_OBJS) \
    $(RUST_OBJS)

all: $(TARGET)

$(TARGET): $(OBJS)
	@printf " LD  %-6s\n" "$@"
	@$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

DEPS := $(OBJS:.o=.d)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf " CC  %-6s\n" "$<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@printf " CXX  %-6s\n" "$<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@printf " AS  %-6s\n" "$<"
	@$(CC) $(COMMON_FLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.rs
	@mkdir -p $(dir $@)
	@printf " RS  %-6s\n" "$<"
	@$(RUSTC) \
        --crate-type lib \
        --emit=obj \
        $(RUSTFLAGS) \
        -o $@ \
        $<


-include $(DEPS)

.PHONY: \
    all \
    clean \
    size \
    sections \
    symbols \
    readelf \
    acpica-only


clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) $(TARGET)

size:
	$(SIZE) $(TARGET)

sections:
	$(OBJDUMP) -h $(TARGET)

symbols:
	$(NM) -n $(TARGET)

readelf:
	$(READELF) -l $(TARGET)

