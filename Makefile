# KittyOS Makefile
#
# Build host: Linux (WSL2 Ubuntu).  QEMU runs natively on Windows, so the
# default `run` target invokes qemu-system-x86_64.exe through WSL interop.

AS      := nasm
CC      := gcc
LD      := ld
OBJCOPY := objcopy

ASFLAGS := -f elf64

# -ffreestanding/-nostdlib : no hosted C environment, no libc, no crt0.
# -mno-red-zone            : MANDATORY.  The System V AMD64 ABI lets leaf
#                            functions use the 128 bytes below RSP (the "red
#                            zone") without adjusting RSP.  The CPU pushes an
#                            interrupt frame at RSP when an interrupt fires, so
#                            in kernel code an interrupt would silently clobber
#                            that region.  Disabling it is not an optimisation
#                            choice, it is a correctness requirement.
# -mno-mmx/-sse/-sse2      : we never enable the FPU/SSE state, so the compiler
#                            must not emit instructions that need it.
# -mcmodel=kernel          : we do not use it for a higher-half layout here, but
#                            it keeps codegen to RIP-relative/32-bit sign-
#                            extended references, which is fine at 1 MiB.
CFLAGS  := -m64 -ffreestanding -fno-builtin -fno-stack-protector -fno-pie \
           -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel \
           -nostdlib -Wall -Wextra -std=c11

LDFLAGS := -m elf_x86_64 -T linker.ld -nostdlib -z max-page-size=0x1000

ASM_SRC := boot.asm
C_SRC   := $(wildcard *.c)
OBJ     := $(ASM_SRC:.asm=.o) $(C_SRC:.c=.o)

KERNEL   := kernel.elf
KERNEL32 := kernel32.elf

# QEMU: Windows-native binary (opens a window on the Windows desktop) by
# default; run-wsl uses the Linux build instead.  Both are overridable, e.g.
#   make run QEMU_WIN='/mnt/c/Program Files/qemu/qemu-system-x86_64.exe'
# for when QEMU is not on the Windows PATH that WSL interop inherits.
QEMU_WIN ?= qemu-system-x86_64.exe
QEMU_LIN ?= qemu-system-x86_64
QEMUFLAGS := -kernel $(KERNEL32) -no-reboot -no-shutdown

# The PC speaker is silent unless an audio backend is bound to it.  Windows
# QEMU uses DirectSound; the Linux build gets PulseAudio (WSLg provides one).
# Set AUDIO= on the command line to mute:  make run AUDIO=
AUDIO_WIN ?= -audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0
AUDIO_LIN ?= -audiodev pa,id=snd0 -machine pcspk-audiodev=snd0

.PHONY: all run run-wsl run-wav debug clean

all: $(KERNEL32)

# QEMU's built-in Multiboot loader refuses ELF64 images on several versions.
# Every load address in this kernel is below 4 GiB, so an ELF32 container
# describes the exact same program headers -- objcopy just rewrites the wrapper.
$(KERNEL32): $(KERNEL)
	$(OBJCOPY) -I elf64-x86-64 -O elf32-i386 $< $@

$(KERNEL): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	$(QEMU_WIN) $(QEMUFLAGS) $(AUDIO_WIN)

run-wsl: all
	$(QEMU_LIN) $(QEMUFLAGS) $(AUDIO_LIN)

# Record the speaker to melody.wav instead of playing it -- useful to check the
# tune without speakers, and how the sound output was verified in the first
# place.  Kill QEMU when you have heard enough; the file is written as it goes.
run-wav: all
	$(QEMU_WIN) $(QEMUFLAGS) -audiodev wav,id=snd0,path=melody.wav \
		-machine pcspk-audiodev=snd0

# Attach with: gdb kernel.elf -ex 'target remote localhost:1234'
debug: all
	$(QEMU_WIN) $(QEMUFLAGS) -s -S

clean:
	rm -f *.o $(KERNEL) $(KERNEL32)
