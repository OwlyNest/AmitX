
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

echo "[+] Cleaning previous build..."
run "make clean"

echo "[+] Building kernel..."
run "make"

echo "[+] Preparing ISO directory..."
run "mkdir -p isodir/boot/grub"
run "cp kernel.bin isodir/boot/kernel.bin"

echo "[+] Creating GRUB config..."
cat > isodir/boot/grub/grub.cfg <<EOF
set timeout=0
set default=0

menuentry \"AmitX Kernel\" {
    multiboot /boot/kernel.bin
    boot
}
EOF

echo "[+] Creating bootable ISO..."
run "grub-mkrescue -o amitx.iso isodir"

echo "[+] Launching QEMU..."
set +e
qemu-system-i386 -cdrom amitx.iso -m 256 -no-reboot -serial stdio -monitor none -device isa-debug-exit,iobase=0xf4,iosize=0x04 -full-screen
QEMU_EXIT=$?
run "make clean"


echo "$QEMU_EXIT"
set -e

case $QEMU_EXIT in
    37)
        echo "[+] Kernel requested: launch Owly"
        echo "[x] HOOT HOOT!"
        ;;
    35)
        echo "[+] Kernel requested: launch Perch"
        cd ../Shell
        chmod +x boot.sh
        ./boot.sh
        ;;
    1)
        echo "[+] Kernel exited gracefully."
        ;;
    0)
        echo "[+] You did the ctrl+C didn't you?"
        ;;
esac