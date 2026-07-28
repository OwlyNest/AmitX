#!/bin/bash

set -e  # Exit on error

VERBOSE=0
REBUILD=1
FULL=1

# Check for flags (break verwijderd zodat ALLE argumenten worden gecontroleerd)
for arg in "$@"; do
    case "$arg" in
        --verbose|-v)
            VERBOSE=1
            ;;
        --no-rebuild|-nr)
            REBUILD=0
            ;;
        --partial|-p)
            FULL=0
            ;;
    esac
done

run() {
    if [[ $VERBOSE -eq 1 ]]; then
        eval "$@"
    else
        eval "$@" > /dev/null 2>&1
    fi
}

if [[ $REBUILD -eq 1 ]]; then
    run ./generate_build_mks.sh

    if [[ $FULL -eq 1 ]]; then
        echo -e "\e[33m[x] Cleaning previous build...\e[0m"
        run "make clean"
    fi
    echo -e "\e[33m[x] Building kernel...\e[0m"
    if [[ $FULL -eq 1 ]]; then
        run "bear -- make -j"
    else
        run "make -j"
    fi
    find . -name '*.c' -o -name '*.h' -o -name '*.S' | sed 's/.*/"&"/' | xargs wc -l | tail -n 1
    make size
    make readelf
    make sections

    # Structure directories directly onto the virtualized architecture
    echo -e "\e[33m[x] Preparing ISO directory structure...\e[0m"
    run "mkdir -p isodir/boot/grub"
    run "cp kernel.elf isodir/boot/kernel.elf"

    echo -e "\e[33m[x] Creating GRUB config...\e[0m"
    cat > isodir/boot/grub/grub.cfg <<EOF
set timeout=0
set default=0

menuentry "AmitX Kernel" {
    multiboot2 /boot/kernel.elf
    boot
}
EOF

    echo -e "\e[33m[x] Fabricating Hybrid ISO (No Sudo Required)...\e[0m"
    # Using raw xorriso flags ensures a standard ISO layout contains an embedded MBR partition layout
    run "grub-mkrescue -o amitx.iso isodir -- -as mkisofs -graft-points -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table --grub2-mbr /usr/lib/grub/i386-pc/boot_hybrid.img"

    cp amitx.iso /mnt/c/Users/23066776/AmitX/
fi

echo -e "\e[33m[x] Launching QEMU (Testing Image)...\e[0m"
set +e

GDK_BACKEND=x11 qemu-system-i386 \
    -cpu Haswell \
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
    -rtc base=localtime
    # -d int,cpu_exit # reveal your secrets

QEMU_EXIT=$?

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