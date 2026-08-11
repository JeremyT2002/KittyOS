; boot.asm -- Multiboot header, 32-bit stub, long mode transition, IRQ stubs.
;
; A Multiboot loader (QEMU's -kernel) enters _start in 32-bit protected mode:
;   * paging OFF, interrupts OFF
;   * a flat GDT of unknown layout (we must load our own before we rely on it)
;   * EAX = 0x2BADB002, EBX = pointer to the multiboot info struct
;
; Getting to 64-bit long mode requires, in this order:
;   1. prove the CPU supports it (CPUID, then CPUID leaf 0x80000001 EDX bit 29)
;   2. build page tables -- long mode has no unpaged variant, paging is required
;   3. CR4.PAE on, EFER.LME on, CR0.PG on   (this puts us in compatibility mode)
;   4. load a 64-bit GDT and far-jump through a 64-bit code selector

bits 32

; ---------------------------------------------------------------------------
; Multiboot 1 header -- 4-byte aligned, within the first 8 KiB of the image.
; The linker script emits .multiboot at the very start of .text.
; ---------------------------------------------------------------------------
MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x00000003          ; bit0 = align modules, bit1 = memory info
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

; ---------------------------------------------------------------------------
; 32-bit entry
; ---------------------------------------------------------------------------
section .text
global _start

_start:
    cli                             ; the loader should have done this already
    mov esp, stack_top              ; a stack we own, not the loader's

    call check_cpuid
    call check_long_mode

    call setup_page_tables
    call enable_paging

    ; With CR0.PG and EFER.LME set the CPU is in long mode, but the current CS
    ; still describes a 32-bit segment (compatibility mode).  Loading a GDT with
    ; an L=1 code descriptor and far-jumping through it is what actually widens
    ; execution to 64 bits.
    lgdt [gdt64.pointer]
    jmp GDT64_CODE:long_mode_start

; --- error path -------------------------------------------------------------
; Prints "ERR:" plus a one-character code at the top-left of the VGA text buffer
; and halts forever.  AL holds the code on entry.
error:
    mov dword [0xB8000], 0x4F524F45 ; 'E','R' white-on-red (attr 0x4F)
    mov dword [0xB8004], 0x4F3A4F52 ; 'R',':'
    mov byte  [0xB8008], al
    mov byte  [0xB8009], 0x4F
.halt:
    hlt
    jmp .halt

; --- CPUID present? ---------------------------------------------------------
; CPUID exists iff software can toggle EFLAGS bit 21 (the ID flag).
check_cpuid:
    pushfd
    pop eax
    mov ecx, eax                    ; keep the original for comparison
    xor eax, 1 << 21                ; flip ID
    push eax
    popfd
    pushfd                          ; read it back
    pop eax
    push ecx                        ; restore the original EFLAGS either way
    popfd
    cmp eax, ecx
    je .no_cpuid                    ; bit did not stick -> no CPUID
    ret
.no_cpuid:
    mov al, '0'
    jmp error

; --- long mode supported? ---------------------------------------------------
check_long_mode:
    mov eax, 0x80000000             ; largest extended leaf supported
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode                ; leaf 0x80000001 does not even exist

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29               ; EDX bit 29 = LM (long mode)
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, '1'
    jmp error

; --- page tables ------------------------------------------------------------
; Identity-map the first 1 GiB with 2 MiB pages:
;   PML4[0] -> PDPT,  PDPT[0] -> PD,  PD[i] -> 2 MiB page at i * 2 MiB
; 1 GiB is far more than we need but costs exactly three 4 KiB tables and no
; run-time bookkeeping, and it covers the framebuffer at 0xB8000.
; Entry flag bits: 0 = present, 1 = writable, 7 = PS (this entry maps a large
; page rather than pointing at another table).
setup_page_tables:
    mov eax, pdpt
    or  eax, 0b11                   ; present | writable
    mov [pml4], eax

    mov eax, pd
    or  eax, 0b11                   ; present | writable
    mov [pdpt], eax

    xor ecx, ecx                    ; ecx = page index
.map_pd:
    mov eax, 0x200000               ; 2 MiB
    mul ecx                         ; eax = physical address of this page
    or  eax, 0b10000011             ; present | writable | huge page
    mov [pd + ecx * 8], eax         ; low dword; high dword stays 0 (bss)
    inc ecx
    cmp ecx, 512                    ; 512 * 2 MiB = 1 GiB
    jne .map_pd
    ret

; --- turn on PAE, LME and paging -------------------------------------------
enable_paging:
    mov eax, pml4
    mov cr3, eax                    ; CR3 = physical address of the PML4

    mov eax, cr4
    or  eax, 1 << 5                 ; CR4.PAE -- 64-bit page table entries
    mov cr4, eax

    mov ecx, 0xC0000080             ; IA32_EFER
    rdmsr
    or  eax, 1 << 8                 ; EFER.LME -- long mode enable
    wrmsr

    mov eax, cr0
    or  eax, 1 << 31                ; CR0.PG -- paging on; with LME set this
    or  eax, 1 << 0                 ; CR0.PE -- (already on, set for clarity)
    mov cr0, eax                    ; ... activates long mode
    ret

; ---------------------------------------------------------------------------
; 64-bit entry
; ---------------------------------------------------------------------------
bits 64
long_mode_start:
    ; In long mode the data segment registers are mostly ignored, but their
    ; cached descriptors are stale 32-bit ones; load the flat data selector so
    ; nothing inherits garbage.
    mov ax, GDT64_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack_top

    ; Hand over to C.  The System V AMD64 ABI wants RSP 16-byte aligned at the
    ; point of a CALL, which stack_top (16-byte aligned in .bss) satisfies.
    extern kernel_main
    call kernel_main

.hang:
    hlt
    jmp .hang

; ---------------------------------------------------------------------------
; Interrupt stubs.  The CPU pushes only SS:RSP, RFLAGS, CS:RIP; preserving
; general purpose registers is the handler's job.  A C function may clobber any
; caller-saved ("volatile") register, so those are exactly the ones we save:
; RAX, RCX, RDX, RSI, RDI, R8-R11.  The callee-saved set is preserved by the C
; compiler itself.
;
; (This is also why the kernel is compiled with -mno-red-zone: without it, the
; interrupted function could be using the 128 bytes below RSP, and the CPU's
; interrupt frame lands right on top of them.)
; ---------------------------------------------------------------------------
global irq0_stub
global isr_default_stub
extern pit_tick

%macro PUSH_VOLATILE 0
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
%endmacro

%macro POP_VOLATILE 0
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
%endmacro

irq0_stub:
    PUSH_VOLATILE
    sub rsp, 8                      ; 9 pushes is odd; keep RSP 16-byte aligned
    cld                             ; the SysV ABI requires DF = 0 on entry
    call pit_tick
    add rsp, 8

    ; End Of Interrupt.  Until the PIC receives EOI it will not raise this or
    ; any lower-priority IRQ again, so a missing EOI silently stops the timer.
    mov al, 0x20
    out 0x20, al                    ; 0x20 = master PIC command port

    POP_VOLATILE
    iretq                           ; 64-bit interrupt return (pops the 5-qword
                                    ; frame the CPU pushed)

; Catch-all for every vector we do not handle: paint a red '!' in the top-right
; corner and stop, instead of triple-faulting into an endless reboot.
isr_default_stub:
    mov word [0xB8000 + 79 * 2], 0x4F21     ; '!' white on red
.halt:
    cli
    hlt
    jmp .halt

; ---------------------------------------------------------------------------
; 64-bit GDT.  In long mode the base/limit fields are ignored for code and data;
; only the flag bits matter.
;   bit 41 = writable (data)      bit 43 = executable (code)
;   bit 44 = descriptor type (1 = code/data, not a system descriptor)
;   bit 47 = present              bit 53 = L (64-bit code segment)
; ---------------------------------------------------------------------------
section .rodata
gdt64:
    dq 0                                            ; mandatory null descriptor
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)        ; 64-bit code, DPL 0
.data: equ $ - gdt64
    dq (1<<41) | (1<<44) | (1<<47)                  ; data, writable, DPL 0
.pointer:
    dw $ - gdt64 - 1                                ; limit = size - 1
    dq gdt64                                        ; base

GDT64_CODE equ gdt64.code
GDT64_DATA equ gdt64.data

; ---------------------------------------------------------------------------
; Page tables and stack.  Page tables must be 4 KiB aligned because the low 12
; bits of a table address in CR3/entries are flag bits, not address bits.
; ---------------------------------------------------------------------------
section .bss
alignb 4096
pml4:       resb 4096
pdpt:       resb 4096
pd:         resb 4096

alignb 16
stack_bottom:
    resb 16384                      ; 16 KiB kernel stack
stack_top:

; Tell the linker this object does not need an executable stack.  Without this
; marker GNU ld warns ("missing .note.GNU-stack section implies executable
; stack") on every link -- harmless here, but the build should be warning-free.
section .note.GNU-stack noalloc noexec nowrite progbits
