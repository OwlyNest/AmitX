# AmitX Architecture

## Current tree

```none
.
├── arch
│   └── x86
├── boot
├── drivers
├── exec
├── fs
├── gfx
├── hw
├── kernel
├── lib
├── logo
├── mm
├── screen
├── shell
├── tests
└── ui
```

## Kernel

boot/
mm/
kernel/
arch/
drivers/
hw/
exec/
fs/

## Operating System

screen/
shell/
tests/
ui/
lib/
logo/
gfx/

| #      | Name                | ebx          | ecx                | edx            | Returns |
| ------ | ------------------- | ------------ | ------------------ | -------------- | ------- |
| `0x50` | `WIN_CREATE`        | w            | h                  | flags          | handle  |
| `0x51` | `WIN_DESTROY`       | handle       | —                  | —              | 0       |
| `0x52` | `WIN_SHOW`          | handle       | —                  | —              | 0       |
| `0x53` | `WIN_HIDE`          | handle       | —                  | —              | 0       |
| `0x54` | `WIN_SET_TITLE`     | handle       | ptr                | —              | 0       |
| `0x55` | `WIN_SET_ACTIVE`    | handle       | —                  | —              | 0       |
| `0x56` | `WIN_GET_ACTIVE`    | —            | —                  | —              | handle  |
| `0x57` | `WIN_GET_SURFACE`   | handle       | out_ptr            | —              | 0       |
| `0x58` | `WIN_GET_PITCH`     | handle       | —                  | —              | pitch   |
| `0x59` | `WIN_GET_DIMS`      | handle       | `&w`               | `&h`           | 0       |
| `0x5A` | `WIN_CLEAR`         | handle       | color              | —              | 0       |
| `0x5B` | `WIN_PRESENT`       | handle       | —                  | —              | 0       |
| `0x5C` | `MOUSE_POS`         | `&x`         | `&y`               | —              | 0       |
| `0x5D` | `MOUSE_BUTTONS`     | —            | —                  | —              | mask    |
