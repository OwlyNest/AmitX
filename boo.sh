
#!/bin/bash

set -e  # Exit on error

VERBOSE=0

# Check for --verbose flag
for arg in "$@"; do
    if [[ "$arg" == "--verbose" || "$arg" == "-v" ]]; then
        VERBOSE=1
        break
    fi
done
run() {
    if [[ $VERBOSE -eq 1 ]]; then
        eval "$@"
    else
        eval "$@" > /dev/null 2>&1
    fi
}

echo -e "\e[33m[x] Cleaning previous build...\e[0m"
run "make clean"

echo -e "\e[33m[x] Building kernel...\e[0m"
run "make"

echo -e "\e[33m[x] Preparing ISO directory...\e[0m"
run "mkdir -p isodir/boot/grub"
run "cp kernel.bin isodir/boot/kernel.bin"

echo -e "\e[33m[x] Creating GRUB config...\e[0m"
cat > isodir/boot/grub/grub.cfg <<EOF
set timeout=0
set default=0

menuentry \"AmitX Kernel\" {
    multiboot /boot/kernel.bin
    boot
}
EOF

echo -e "\e[33m[x] Creating bootable ISO...\e[0m"
run "grub-mkrescue -o amitx.iso isodir"

echo -e "\e[33m[x] Launching QEMU...\e[0m"
set +e
qemu-system-i386 -cdrom amitx.iso -m 256 -no-reboot -serial stdio -monitor none -device isa-debug-exit,iobase=0xf4,iosize=0x04 -full-screen
QEMU_EXIT=$?
run "make clean"


echo "$QEMU_EXIT"
set -e

case $QEMU_EXIT in
    37)
        echo -e "\e[33m[x] Kernel requested: launch Owly\e[0m"
        echo -e "\e[33m[x] HOOT HOOT!\e[0m"
        ;;
    35)
        echo -e "\e[33m[x] Kernel requested: launch Perch\e[0m"
        cd ../Shell
        chmod +x boot.sh
        ./boot.sh
        ;;
    1)
        echo -e "\e[33m[x] Kernel exited gracefully.\e[0m"
        ;;
    0)
        echo -e "\e[33m[x] You did the ctrl+C didn't you?\e[0m"
        ;;
esac