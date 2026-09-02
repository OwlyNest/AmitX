# --------------------------------------------------------------------
# Phonon Makefile
#
# Windows-native build:
# PowerShell + GNU Make
# --------------------------------------------------------------------

# --------------------------------------------------------------------
# Shell
# --------------------------------------------------------------------
ifeq ($(OS), Windows_NT)
SHELL := powershell.exe
.SHELLFLAGS := -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command
else
SHELL := /usr/bin/pwsh
.SHELLFLAGS := -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command
endif

# --------------------------------------------------------------------
# Architecture / Toolchain
# --------------------------------------------------------------------

ARCH ?= x86
ifeq ($(ARCH),x86)
    TOOLCHAIN := i686-elf-
    ARCH_FLAGS := -m32
    ARCH_DEFINE := -DPHONON_ARCH_X86
else ifeq ($(ARCH),x64)
    TOOLCHAIN := x86_64-elf-
    ARCH_FLAGS := -m64 -mno-red-zone
    ARCH_DEFINE := -DPHONON_ARCH_X64
else
    $(error Unsupported ARCH '$(ARCH)'. Use ARCH=x86 or ARCH=x64)
endif

CC       := $(TOOLCHAIN)gcc
C3       := c3c
CXX      := $(TOOLCHAIN)g++
LD       := $(TOOLCHAIN)ld
AS       := $(CC)
AR       := $(TOOLCHAIN)ar
NM       := $(TOOLCHAIN)nm
SIZE     := $(TOOLCHAIN)size
OBJDUMP  := $(TOOLCHAIN)objdump
OBJCOPY  := $(TOOLCHAIN)objcopy
READELF  := $(TOOLCHAIN)readelf
RUSTC    ?= rustc
CARGO    ?= cargo

# --------------------------------------------------------------------
# Windows filesystem helpers
# --------------------------------------------------------------------

ifeq ($(OS),Windows_NT)

define RM_DIR
    if (Test-Path -LiteralPath '$(1)') { Remove-Item -LiteralPath '$(1)' -Recurse -Force }
endef

define RM_FILE
    if (Test-Path -LiteralPath '$(1)') { Remove-Item -LiteralPath '$(1)' -Force }
endef

else

define RM_DIR
    rm -rf "$(1)"
endef

define RM_FILE
    rm -f "$(1)"
endef

endif

# --------------------------------------------------------------------
# Project layout
# --------------------------------------------------------------------

TARGET       := kernel.elf
BUILD_DIR    := build
LINKER_SCRIPT := boot/linker-$(ARCH).ld

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

# NOTE: arch/x86 is NOT in this list. Its build.mk handles 32/64/common.
SRC_DIRS := \
    shell \
    fs \
    fs/smkfs \
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

# --------------------------------------------------------------------
# Compiler flags
# --------------------------------------------------------------------

COMMON_FLAGS := \
    $(ARCH_FLAGS) \
    $(ARCH_DEFINE) \
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

ifeq ($(ARCH),x64)
COMMON_FLAGS += -mcmodel=kernel
endif

CFLAGS := \
    $(COMMON_FLAGS) \
    -Wall \
    -Wextra \
    -Werror \
    -include include/internal/phonon_types.h \
    -isystem $(ACPICA_INC)

C3FLAGS := \
    --target elf-$(ARCH) \
    --use-stdlib=no \
    --emit-stdlib=no \
    -g0

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
    -nostdlib -z noexecstack

LIBGCC := $(shell $(CC) -print-libgcc-file-name)
LIBS := $(LIBGCC)

# --------------------------------------------------------------------
# Generated build files
# --------------------------------------------------------------------

-include shell/build.mk
-include fs/build.mk
-include fs/smkfs/build.mk
-include arch/x86/build.mk      # Hand-written selector for 32/64/common
-include drivers/build.mk
-include lib/build.mk
-include mm/build.mk
-include kernel/build.mk
-include screen/build.mk
-include tests/build.mk
-include logo/build.mk
-include ui/build.mk
-include hw/build.mk
-include gfx/build.mk
-include exec/build.mk
-include sync/build.mk
-include acpi/build.mk
-include acpica.mk

# --------------------------------------------------------------------
# Object lists
# --------------------------------------------------------------------

C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
C3_OBJS := $(patsubst %.c3,$(BUILD_DIR)/%.o,$(C3_SRCS))
CPP_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CPP_SRCS))
ASM_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))
ACPICA_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(ACPICA_SRCS))
RUST_OBJS := $(patsubst %.rs,$(BUILD_DIR)/%.o,$(RUST_SRCS))

$(ACPICA_OBJS): CFLAGS := $(ACPICA_CFLAGS)

OBJS := \
    $(C_OBJS) \
    $(C3_OBJS) \
    $(CPP_OBJS) \
    $(ASM_OBJS) \
    $(ACPICA_OBJS) \
    $(RUST_OBJS)

DEPS := $(OBJS:.o=.d)

# --------------------------------------------------------------------
# Targets
# --------------------------------------------------------------------

.PHONY: all clean size sections symbols readelf acpica-only

all: $(TARGET)

# --------------------------------------------------------------------
# Link
# --------------------------------------------------------------------

# Preprocess linker script
boot/linker-$(ARCH).ld: boot/linker.ld.in
	@Write-Host "GEN $@"
	@& "$(CC)" $(ARCH_DEFINE) -E -P -x c "$<" -o "$@"

# Depend on the generated script
$(TARGET): $(OBJS) boot/linker-$(ARCH).ld
	@Write-Host "LD  $@"
	@& "$(LD)" $(LDFLAGS) -T boot/linker-$(ARCH).ld -o "$@" $(filter-out %.ld,$^) $(LIBS)
# --------------------------------------------------------------------
# C
# --------------------------------------------------------------------

$(BUILD_DIR)/%.o: %.c
	@New-Item -ItemType Directory -Force -Path "$(dir $@)" | Out-Null
	@Write-Host "CC  $<"
	@& "$(CC)" $(CFLAGS) -c "$<" -o "$@"

# --------------------------------------------------------------------
# C3
# --------------------------------------------------------------------

$(BUILD_DIR)/%.o: %.c3
	@New-Item -ItemType Directory -Force -Path "$(dir $@)" | Out-Null
	@Write-Host "C3  $<"
	@& "$(C3)" compile-only "$<" $(C3FLAGS) --obj-out "$(dir $@)" --emit-only "$(basename $(notdir $<))"

# --------------------------------------------------------------------
# C++
# --------------------------------------------------------------------

$(BUILD_DIR)/%.o: %.cpp
	@New-Item -ItemType Directory -Force -Path "$(dir $@)" | Out-Null
	@Write-Host "CXX $<"
	@& "$(CXX)" $(CXXFLAGS) -c "$<" -o "$@"

# --------------------------------------------------------------------
# Assembly
# --------------------------------------------------------------------

$(BUILD_DIR)/%.o: %.S
	@New-Item -ItemType Directory -Force -Path "$(dir $@)" | Out-Null
	@Write-Host "AS  $<"
	@& "$(CC)" $(COMMON_FLAGS) -c "$<" -o "$@"

# --------------------------------------------------------------------
# Rust
# --------------------------------------------------------------------

$(BUILD_DIR)/%.o: %.rs
	@New-Item -ItemType Directory -Force -Path "$(dir $@)" | Out-Null
	@Write-Host "RS  $<"
	@& "$(RUSTC)" --crate-type lib --emit=obj $(RUSTFLAGS) -o "$@" "$<"

# --------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------

-include $(DEPS)

# --------------------------------------------------------------------
# Cleaning
# --------------------------------------------------------------------

clean:
	@Write-Host "Cleaning $(ARCH) build..."
	$(call RM_DIR,$(BUILD_DIR))
	$(call RM_FILE,$(TARGET))
	$(call RM_FILE,boot/linker-$(ARCH).ld)

# --------------------------------------------------------------------
# Inspection
# --------------------------------------------------------------------

size:
	@& "$(SIZE)" "$(TARGET)"

sections:
	@& "$(OBJDUMP)" -h "$(TARGET)"

symbols:
	@& "$(NM)" -n "$(TARGET)"

readelf:
	@& "$(READELF)" -l "$(TARGET)"