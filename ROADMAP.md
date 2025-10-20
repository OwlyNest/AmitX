# ROADMAP.md  
**AmitX — the OS that boots with attitude**  
*By Amity. Because why not build a whole dang operating system?*

---

## GOAL

To build a delightfully handcrafted OS from scratch — one that boots, blinks, hoots, and eventually runs its own shell, compiler, and apps, all while looking fabulous doing it.

I'm not cloning UNIX.  
I'm not rewriting Linux.  
I'm building **AmitX** — the OS that perches on your CPU and screams into the void with elegance.

---

## STAGE 0.1 – “Kernel Kindergarten”

**Status: Mostly Hatched**

> The stage where everything breaks constantly and you're proud of it anyway.

### Already nailed it:
- [x] Multiboot-friendly kernel boots like a champ
- [x] Screen output via VGA text mode (pixels? we don’t know her… yet)
- [x] Keyboard interrupts working — press ‘w’, feel powerful
- [x] Basic IDT, PIC remapping, and IRQ setup
- [x] A very dramatic boot screen (`screen_puts` is the real MVP)
- [x] "Perch" menu system with item highlighting and selection
- [x] `hlt`-based “exit” that feels like a mic drop
- [x] Refactor IDT setup to `idt.c` — cleaner than a hoot’s wing
- [x] Build app launcher: press Enter, run app from memory
- [x] Proper scancode buffer so we can *actually* type things

### Up Next:
- [] Log macros (`klog_info`, `klog_hoot`, etc.) for extra flair

---

## STAGE 0.2 – “Featherweight Framework”

> You ever seen a bird build a house? We're building a whole operating system.
### Already flying:
- [x] A `malloc` and `free` (don’t get greedy)
- [x] A kernel heap, because global variables are for cowards
- [x] Simple paging or at least basic memory protection

### Upcoming sorcery:
- [ ] Better input: command buffer, editing, history
- [ ] Write-only dreams: kernel logs and debug info to screen or serial

---

## STAGE 0.3 – “Owly Gets Weird”

> Time to teach Owly to hoot back.

### Target:
- [ ] Owly compiler supports real syntax (vars, if, while, print)
- [ ] Add bytecode or AST runner in kernel land
- [ ] Owly files as kernel “apps” run from memory
- [ ] Write an Owly-powered app. Something silly. Something great.
- [ ] Begin: `owlyc` compiled in Owly itself — a snake eating its tail (but make it a bird)

---

## STAGE 0.4 – “Perch Evolves”

> Not your grandma’s shell (but still would make her proud)

### Ambitions:
- [ ] Rewrite Perch as a freestanding app (no GTK, no host dependencies)
- [ ] Create minimal libc-like helpers (string, stdio-style, math)
- [ ] Build a mini terminal emulator within VGA (Perch TUI)
- [ ] Add useful commands: `uptime`, `hoot`, `owly run`, `perch ls`
- [ ] Add nested menu or app launcher from within Perch

---

## STAGE 0.5 – “Storage is a Suggestion”

> QEMU is fun, but persistent files? Yes, please.

### Bold ideas:
- [ ] Read from disk (FAT32 or tiny in-memory FS like `hootfs`)
- [ ] Add app loader: copy `Perch`, `Owly`, etc. into ISO
- [ ] Syscall-like interface to request disk I/O from kernel
- [ ] Implement a file viewer in Perch (so meta)

---

## STAGE ∞ – “Dream Bigger”

> We’re deep in the void now.

- [ ] Graphical mode (VGA/VESA) + mouse support
- [ ] Perch with GUI buttons, draggable windows, owl cursor
- [ ] Preemptive multitasking — actually run two apps!
- [ ] Networking stack (ping `hoot.land`)
- [ ] Owly-written kernel modules
- [ ] Custom bootloader: ditch GRUB like a bad date

---

## Philosophy

- Keep it personal. Keep it fun.
- Learn deeply, build weirdly, and never forget to hoot.
- The OS is a canvas, not a cage.

---

## Appendix: Components

| Folder         | Purpose                                  |
|----------------|-------------------------------------------|
| `Kernel/`      | Bootloader, IDT, core logic, VGA, menu    |
| `Owly/`        | Custom compiler (aka bird wizardry)       |
| `Shell/`       | Perch (GTK now, freestanding soon)        |
| `amitx.iso`    | Built ISO image for booting               |
| `requirements.sh` | Setup helpers & build dependencies   |

---

Built with grit, giggles, and a little bit of chaos.  
— Amity