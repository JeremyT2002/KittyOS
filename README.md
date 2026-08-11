# KittyOS

A minimal, bootable x86-64 operating system whose entire purpose in life is to
fly an ASCII cat across the screen trailing a rainbow, forever, in VGA text
mode.

No bootloader of its own, no GRUB ISO, no libc, no heap. It boots via a
Multiboot 1 header, switches itself from 32-bit protected mode into 64-bit long
mode, programs the PIC and PIT, and then renders 12.5 frames per second into a
back buffer that it flips to `0xB8000`.

```
  ████████████████████ ▄▄▄▄▄▄
  ████████████████████ █    █  /\_/\
  ████████████████████ █ ·  █ ( o.o )   <- roughly what you get, in 16 colours
  ████████████████████ ▀▀▀▀▀▀   > ^ <
```

## Build environment

Built on Linux (WSL2 Ubuntu), run with the Windows-native QEMU.

```bash
sudo apt update
sudo apt install -y nasm build-essential binutils make
```

`build-essential` pulls in `gcc` and `make`; `binutils` gives you `ld` and
`objcopy`. Nothing else is required — no cross-compiler, because the host `gcc`
already targets x86-64 and everything is built freestanding.

```bash
make          # -> kernel.elf and kernel32.elf
make run      # launches qemu-system-x86_64.exe (window opens on the Windows side)
make run-wsl  # launches the Linux qemu-system-x86_64 instead
make run-wav  # plays to melody.wav instead of the speakers
make debug    # same as run, plus -s -S so you can attach gdb
make clean
```

`make run` calls `qemu-system-x86_64.exe`, which works from WSL as long as
Windows PATH interop is on (the default) and QEMU is on the Windows `PATH`. If
it is not, either add `C:\Program Files\qemu` to your Windows PATH or override
the variable for one run:

```bash
make run QEMU_WIN='/mnt/c/Program Files/qemu/qemu-system-x86_64.exe'
```

For gdb:

```bash
make debug &
gdb kernel.elf -ex 'target remote localhost:1234'   # note: kernel.elf, not kernel32.elf
```

Use `kernel.elf` for debugging — it is the ELF64 image and carries the symbols
in the form gdb expects for a 64-bit target.

### ⚠️ CRLF line endings

Every file here is written with Unix (LF) line endings and must stay that way:

* **`Makefile`** — a recipe line ending in `\r` makes the shell see a command
  with a stray carriage return; you get errors like `/bin/sh: 1: gcc: not
  found` or `missing separator` that make no sense.
* **`linker.ld`** and **`boot.asm`** are tolerant of CRLF in practice, but there
  is no reason to risk it.

Windows editors and `git` on Windows are the usual culprits. A `.gitattributes`
is included that pins the whole tree to LF. If a file has already been
converted, fix it with:

```bash
sudo apt install -y dos2unix && dos2unix Makefile
# or check first:
file Makefile        # should NOT say "with CRLF line terminators"
```

## Why there are two kernels

`make` produces both `kernel.elf` (ELF64) and `kernel32.elf`, and QEMU is booted
with the latter. This is not optional paranoia — QEMU's built-in Multiboot
loader refuses ELF64 images:

```
qemu-system-x86_64: Cannot load x86-64 image, give a 32bit one.
```

(verified on QEMU 10.1.94). The fix is to rewrap the same program headers in an
ELF32 container:

```
objcopy -I elf64-x86-64 -O elf32-i386 kernel.elf kernel32.elf
```

This is purely a change of container. It is safe because every load address in
this kernel is below 4 GiB, so no address is truncated, and the actual machine
code inside is untouched 64-bit code. The Multiboot header tells the loader
where to put it; the loader never needs to understand the code.

## Files

| File | What it does |
| --- | --- |
| `boot.asm` | Multiboot header, 32-bit stub, long mode transition, 64-bit entry, IRQ stubs |
| `linker.ld` | Entry `_start`, sections laid out from 1 MiB |
| `kernel.c` | `kernel_main`, frame pacing loop, `memset`/`memcpy` |
| `vga.c` / `vga.h` | 80x25 text mode driver and back buffer |
| `nyan.c` / `nyan.h` | Scene composition: rainbow trail, cat, starfield |
| `nyan_art.h` | **Generated** cat sprite: 4 frames, 30x20 pixels |
| `splash.c` / `splash.h` | Boot splash: the same cat at full size |
| `tools/braille2sprite.py` | Braille art -> sprite generator (build-time only, not part of the kernel) |
| `tools/cat.txt` | The original Braille art |
| `idt.c` / `idt.h` | 64-bit IDT construction, 8259 PIC remap |
| `pit.c` / `pit.h` | PIT channel 0 at 100 Hz, tick counter |
| `speaker.c` / `speaker.h` | PC speaker tones and the melody sequencer |
| `io.h` | `inb`/`outb`/`io_wait` (header-only; needed by vga, idt and pit alike) |

## How it works

### Boot and the long mode transition (`boot.asm`)

The Multiboot loader enters `_start` in 32-bit protected mode with paging off
and interrupts off. Getting to 64-bit code takes five steps, in this order:

1. **Check CPUID exists** by toggling EFLAGS bit 21 (the ID flag) and seeing
   whether the bit sticks.
2. **Check long mode exists** — CPUID leaf `0x80000001`, EDX bit 29. If either
   check fails the kernel paints `ERR:0` or `ERR:1` white-on-red at the top-left
   and halts, rather than executing an invalid instruction.
3. **Build page tables.** Long mode has no unpaged variant, so paging must be on
   before the switch. `PML4[0] -> PDPT[0] -> PD`, with the PD using 2 MiB pages
   (bit 7, `PS`) to identity-map the first 1 GiB. Three 4 KiB-aligned tables in
   `.bss`, no run-time bookkeeping, and 1 GiB comfortably covers both the kernel
   at 1 MiB and the framebuffer at `0xB8000`.
4. **Flip the switches:** `CR3` = PML4, `CR4.PAE` (bit 5), `EFER.LME` via MSR
   `0xC0000080` (bit 8), then `CR0.PG` (bit 31). At this instant the CPU is in
   long mode — but in *compatibility mode*, because CS still describes a 32-bit
   segment.
5. **Load a 64-bit GDT and far jump** through its code selector. The `L` bit
   (bit 53) in the code descriptor is what actually widens execution to 64 bits.
   In long mode the base/limit fields are ignored; only the flag bits matter.

The 64-bit entry reloads the data segment registers, sets up a 16 KiB stack from
`.bss`, and calls `kernel_main`.

### Why `-mno-red-zone` is mandatory

The System V AMD64 ABI reserves 128 bytes below `RSP` — the *red zone* — that a
leaf function may use as scratch without adjusting `RSP`. That works in
userspace because nothing else ever writes below the stack pointer. In kernel
code it is fatal: when an interrupt fires, the CPU pushes its 5-qword frame
starting at the current `RSP`, straight through the red zone, silently
corrupting whatever the interrupted function had parked there. Since this kernel
takes a timer interrupt 100 times a second, the corruption would be constant and
would look like random data loss. `-mno-red-zone` is a correctness requirement,
not an optimisation setting.

### Text mode rendering (`vga.c`)

The framebuffer at `0xB8000` is 80x25 cells of two bytes: character (code page
437) and attribute (bits 0-3 foreground, 4-6 background, bit 7 blink). The
pointer is `volatile` so the compiler cannot decide the stores are dead.

Every frame is composed in `uint16_t back[80*25]` and copied to the hardware in a
single pass in `vga_present()`. Drawing straight to `0xB8000` would let the
display refresh catch a half-drawn frame — visible as tearing and flicker.

One consequence of the attribute layout is that **only colours 0-7 can be used
as a background**, since the background field is 3 bits. Yellow is colour 14, so
the rainbow cannot be painted as background colours. Every solid area is
therefore drawn as character `0xDB` (a full block) in the desired *foreground*
colour, which makes all 16 colours available.

#### Two pixels per cell

`vga_put_half()` turns each character cell into two stacked, independently
coloured pixels. A cell is 9x16 screen pixels and CP437 has half blocks — `0xDF`
fills the top half, `0xDC` the bottom, `0xDB` both — and since foreground and
background are coloured separately, one cell can show two colours split
horizontally. That doubles the vertical resolution to an effective 80x50 grid of
roughly square pixels, at no cost.

The 3-bit background limit comes back here: a cell can only carry one colour
brighter than 7, because the other half has to live in the background field.
`vga_put_half()` deals with it by picking whichever half block puts the
unrepresentable colour in the *foreground* — `0xDF` normally, `0xDC` flipped
when it is the bottom pixel that needs to be bright. Only when both pixels need
a bright colour does it approximate, by dimming the bottom one. The cat's
palette is chosen from colours 0-7 so this case never arises for the sprite.

### The animation (`nyan.c`)

The cat is not hand-drawn character art. It is a **30x20 pixel sprite**
(`nyan_art.h`, generated from the Braille art — see below) rendered two pixels
per cell. So the scene is composed in a pixel plane first and only then folded
onto the character grid:

```
starfield (whole cells)
   -> pixel plane: rainbow trail, then cat over it
   -> fold half-row pairs into cells via vga_put_half()
```

Composing in pixels is what buys the smooth motion: the trail waves in steps of
half a cell and the cat bobs by half a cell, both of which look far better than
whole-cell jumps.

* **Cat**: four frames differing in leg position, tail angle, and a half-cell
  vertical bob. Fixed at column 50; it never moves horizontally. Colours come
  from `key_color()` in `nyan.c`, not from the generated header, so the palette
  can be retuned without regenerating the art.
* **Trail**: six bands — red(4), brown(6), yellow(14), green(2), cyan(3),
  magenta(5) — from the left screen edge to the cat, each one cell tall. Every
  column's band stack is offset 0 or 1 *pixels* by a sawtooth `{0,0,1,1}`
  sampled at `(x + tick)`, so the pattern slides one column left per frame and
  the trail appears to stream away.
* **Transparency**: cells where both pixels are transparent are skipped entirely
  during the blit, so the starfield drawn underneath survives, and the tail
  overlapping the rainbow composites correctly instead of punching a
  space-coloured hole in it.
* **Stars**: fixed positions, brightness alternating between white and dark grey
  every four frames, staggered by index so they do not all pulse together.

Nothing accumulates. The frame counter is `uint32_t` and every use of it inside
the renderer is taken modulo a power of two that divides 2^32, so the wrap after
~4 billion frames is seamless.

### Braille art in VGA text mode (`tools/braille2sprite.py`)

Both the splash and the flying cat come from the same piece of Unicode Braille
art (`tools/cat.txt`), which VGA text mode **cannot display at all** — the
hardware font is code page 437 and contains no Braille glyphs. That is not
fixable at the string level; the character codes simply do not exist in the ROM
font.

What makes it work anyway is that a Braille character is not really a character.
It is a 2x4 dot bitmap with a Unicode code point wrapped around it: dot *n* of
`U+28xx` is bit *n* of the low byte. So the art decodes to a plain bitmap, and a
bitmap is something half blocks can draw:

```
30 Braille cells x 11 rows  ->  60 x 44 dots  ->  (trim)  ->  60 x 39 dots
```

**Splash**: used at full size. Folding row pairs into half blocks gives 60x20
cells, comfortably inside 80x25, drawn as a white outline.

**Flying cat**: 60 cells wide leaves no room for a rainbow, so the generator does
more work:

1. **Flood fill** the non-ink regions and label them — the pop-tart interior, the
   head, the tail, and the four legs each come out as their own region.
2. **Classify small enclosed ink blobs** inside the pop-tart as sprinkles rather
   than outline, so they can be coloured separately.
3. **Downscale 2x** to 30x20 pixels, with outline winning any block it covers at
   least half of, so the lines survive the shrink instead of dissolving.
4. **Generate four frames** by shifting the tail and the individual legs. A limb
   moved *down* is stretched rather than translated — translating it tore a
   one-pixel hole where the leg joins the body. A limb moved *up* is really
   moved, which just tucks it into the body above.

The result is `nyan_art.h`, four frames of 20 half-rows each. Regenerate with:

```bash
python3 tools/braille2sprite.py --header nyan_art.h     # rebuild the sprite
python3 tools/braille2sprite.py --preview               # PNG previews, no QEMU
```

The `--preview` mode renders the scene exactly as the kernel would, straight to
PNG. Iterating on the art that way takes a second instead of a build-and-boot
cycle.

To use different art, drop it in `tools/cat.txt` and rerun — but the region ids
near the top of the script (`TART`, `HEAD`, `TAIL`, `LEGS`) are specific to this
picture and will need re-checking against the map that `regions.py`-style
labelling prints.

### The boot splash (`splash.c`)

On boot, the cat is shown large for 2.5 seconds (`SPLASH_TICKS` = 250 ticks)
before the animation starts and runs forever. To change how long it is held,
edit `SPLASH_TICKS` in `splash.h`; to skip it entirely, delete the
`splash_draw()` block from `kernel_main`.

### Sound (`speaker.c`)

The melody comes out of the **PC speaker**, which needs no driver at all: PIT
channel 2 is wired to the speaker cone instead of to the interrupt controller.
Program it in mode 3 and it emits a square wave at `1193182 / divisor` Hz. Port
`0x61` gates it:

```
bit 0  gate  -- lets channel 2 count
bit 1  data  -- connects channel 2's output to the speaker
```

Both bits must be set for sound, and bits 2-7 are chipset status, so the port is
always read-modify-written rather than assigned.

Channel 2 is a separate counter from channel 0, so the music cannot disturb the
tick that paces the animation — the two share the crystal, nothing else.

The sequencer is a table of notes walked by `speaker_update()`, which is called
from *inside* the frame-wait loop rather than once per frame. A frame is 8
ticks, so calling it per frame would quantise every note to a multiple of 80 ms
and the tune would limp. Each note is 11 ticks of tone plus 1 tick of silence,
the gap being what makes two identical notes in a row audible as two notes.

The melody is a hand-tuned approximation of the Nyan Cat riff, not a
transcription from a score. It is a plain `uint16_t` array of frequencies —
edit it freely.

**Anything above a square wave** (samples, multiple voices, actual audio) needs a
real sound device — Sound Blaster 16, AC'97 or Intel HDA — which means PCI
enumeration and DMA buffer management. That is a substantially bigger project
than this entire kernel, which is why the speaker is where this stops.

#### Hearing it

QEMU keeps the speaker silent unless an audio backend is bound to it, hence the
extra flags in the `run` targets:

```
-audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0     # Windows
-audiodev pa,id=snd0     -machine pcspk-audiodev=snd0     # Linux/WSLg
```

To mute it: `make run AUDIO_WIN=`. To capture the tune to a file instead of
playing it: `make run-wav`, which writes `melody.wav`. Note that QEMU only
patches the RIFF length fields on a clean exit — kill it and you get a valid
stream with zeroed size fields, which some tools refuse to open.

The **tempo follows the tick rate**, so it inherits the host-timer variability
described below: on this Windows host the same phrase measured 80 ms per note
early on and 140 ms later. On a steady 100 Hz it is a constant 110 ms.

### Timing (`idt.c`, `pit.c`)

* The PICs are remapped to vectors `0x20` (master) and `0x28` (slave). On a bare
  PC they deliver IRQ0-7 on vectors 8-15, which collide with the CPU's own
  exception vectors — unusable in long mode.
* Every IRQ is masked except IRQ0: master mask `0xFE`, slave mask `0xFF`.
* The IDT has 256 16-byte gate descriptors, with the handler address split
  across three fields (low 16 / mid 16 / high 32), type `0x8E` (present, DPL 0,
  64-bit interrupt gate), selector `0x08`. Every vector we do not handle points
  at a catch-all stub that paints a red `!` in the top-right corner and stops —
  far easier to diagnose than the reboot loop you would get from a not-present
  gate.
* PIT channel 0, mode 3 (square wave), command `0x36` to port `0x43`, divisor
  11932 (1193182 / 100) written low byte then high byte to port `0x40` — 100 Hz.
* `sti` runs **only after** the IDT is loaded and the PIC is configured. Enable
  interrupts earlier and the first timer tick vectors through a garbage
  descriptor: #GP with no handler, double fault with no handler, triple fault.
* The main loop waits for 8 ticks per frame using `hlt`, so the CPU idles
  between frames instead of spinning.

#### A note on timing under Windows QEMU

The tick rate the guest actually observes under Windows QEMU is not always the
programmed 100 Hz — measured here it ranged from about **65 Hz to 103 Hz**
between runs. The low end is not a PIT misconfiguration: reprogramming the
divisor for 1000 Hz produced the same ~65 Hz, which pins it on Windows' default
15.6 ms timer granularity limiting how precisely QEMU can wake a halted vCPU.

This does not hurt anything. The animation is paced off tick *counts*, so a slow
host yields a slightly slower cat, never a stuttering one. `make run-wsl` under
WSLg, or any Linux host, is steadier.

If you sample the screen programmatically rather than watching it, note that the
entire visual state has a period of exactly 4 frames — sampling on a multiple of
that interval makes a perfectly healthy animation look frozen.

## Debugging

### QEMU keeps rebooting in a loop

That is a **triple fault**: a fault occurred, the handler for it faulted, and
the handler for *that* faulted, so the CPU resets. Run:

```bash
qemu-system-x86_64.exe -kernel kernel32.elf -d int,cpu_reset -no-reboot -no-shutdown
```

`-no-reboot` stops the loop at the first reset so the log is readable. Reading
the output:

* `check_exception old: 0xffffffff new 0xd` — a **#GP (13)** with no prior
  exception. Usually a bad GDT/IDT descriptor or a far jump through a selector
  that is not what you think it is.
* `check_exception old: 0xd new 0xd` or `old: 0xa new 0xd` — the second fault.
  The vector shown as `old` is the one whose handler is broken.
* `v=0e` with an `error_code` and `CR2` in the register dump — a **page fault**.
  Compare `CR2` against the 1 GiB identity mapping; anything at or above
  `0x40000000` is simply not mapped by this kernel.
* `v=08` — a **double fault**, which here almost always means an interrupt fired
  with a not-present or malformed IDT gate. Check that `sti` really does come
  after `idt_init()`.
* `CPU Reset (CPU 0)` followed by `EAX=... EIP=...` — read `EIP`/`RIP` and match
  it against `objdump -d kernel.elf` to find the instruction that killed it.

### The screen stays black

Two very different failures, and the log tells them apart immediately:

* **The kernel never loaded.** QEMU exits or complains straight away:
  `Cannot load x86-64 image, give a 32bit one` (you booted `kernel.elf` instead
  of `kernel32.elf`), or `Error loading uncompressed kernel without PVH ELF
  Note` (the Multiboot header was not found in the first 8 KiB, or its checksum
  is wrong). Verify with:
  ```bash
  objdump -h kernel32.elf              # .text must be the first loaded section
  od -A x -t x1 -j 4096 -N 12 kernel32.elf
  #   expect: 02 b0 ad 1b 03 00 00 00 fb 4f 52 e4
  #           magic 0x1BADB002, flags 3, checksum -- the three must sum to 0
  #           modulo 2^32, and this must land in the first 8 KiB of the image
  ```
  Or check the loader saw it at all: `qemu-system-x86_64.exe -kernel kernel32.elf -d int` prints nothing at all if the image was rejected before execution.
* **The kernel ran but wrote nothing.** QEMU stays up and the window is black.
  Confirm the CPU is alive and where it is stuck by dumping guest memory from
  the monitor (`Ctrl-Alt-2`, or `-monitor stdio`):
  ```
  (qemu) x/4xw 0xb8000     # any non-zero cell means something was written
  (qemu) info registers    # RIP tells you which loop you are parked in
  ```
  If `0xB8000` is all zeroes, the code halted before drawing — most likely in
  the CPUID/long-mode checks, in which case look at the top-left corner for
  `ERR:0` / `ERR:1`, or at the top-right for the red `!` of an unexpected
  interrupt vector.

## Verified

Each stage was booted in QEMU before the next was written:

1. Multiboot header + 32-bit stub writing one coloured character — boots.
2. Long mode transition, a different character written from 64-bit code — boots.
3. VGA driver and back buffer driven from C — boots.
4. Static cat and trail — correct sprite, correct band colours, correct wave.
5. IDT + PIC remap + PIT — tick counter increments, no stray vectors.
6. Full animation — successive frames confirmed to differ in trail phase, leg
   position, body bob and tail angle.
7. Boot splash — renders, holds, and hands over to the animation cleanly.
8. Braille-derived pixel sprite — screenshot from QEMU matches the generator's
   own preview render exactly; 8 successive captures showed 7 distinct screen
   states, confirming the 4-frame pose cycle and the 8-frame star twinkle.
9. PC speaker — recorded to WAV and measured by zero-crossing rate. The emitted
   frequencies matched the note table within the analysis resolution
   (625/650/725/1000/1100/1250 Hz against D#5 622, E5 659, F#5 740, B5 988,
   C#6 1109, D#6 1245), in the programmed order.

The build is warning-free with `-Wall -Wextra`.
# KittyOS
