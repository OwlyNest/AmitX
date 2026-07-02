
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

run ./generate_build_mks.sh

echo -e "\e[33m[x] Cleaning previous build...\e[0m"
run "bear -- make clean"

echo -e "\e[33m[x] Building kernel...\e[0m"
run "bear -- make"
make size
make readelf
make sections
echo -e "\e[33m[x] Preparing ISO directory...\e[0m"
run "mkdir -p isodir/boot/grub"
run "cp kernel.elf isodir/boot/kernel.elf"

echo -e "\e[33m[x] Creating GRUB config...\e[0m"
cat > isodir/boot/grub/grub.cfg <<EOF
set timeout=0
set default=0

menuentry \"AmitX Kernel\" {
    multiboot2 /boot/kernel.elf
    boot
}
EOF

echo -e "\e[33m[x] Creating bootable ISO...\e[0m"
run "grub-mkrescue -o amitx.iso isodir"

echo -e "\e[33m[x] Launching QEMU...\e[0m"
set +e

GDK_BACKEND=x11 qemu-system-i386 \
    -cdrom amitx.iso \
    -drive file=disk.img,format=raw,if=ide \
    -boot d \
    -m 512 \
    -no-reboot \
    -serial stdio \
    -monitor none \
    -machine pc \
    -device vmware-svga \
    -display gtk,full-screen=on,zoom-to-fit=on \
    -rtc base=localtime \
    #-d int,cpu_reset
    
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

stty sane