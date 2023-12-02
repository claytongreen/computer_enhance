; ==============================================================================

global mov_all_bytes_asm
global nop_all_bytes_asm
global nop3_all_bytes_asm
global nop9_all_bytes_asm
global nop32_all_bytes_asm

section .text

%define nop3 db 0x0f, 0x1f, 0x00

; ==============================================================================

; rcx uint8_t *buffer
; rdx uint64_t buffer_length

mov_all_bytes_asm:
  xor rax, rax
.loop:
  mov [rcx + rax], al
  inc rax
  cmp rax, rdx
  jb .loop
  ret

; ==============================================================================

nop_all_bytes_asm:
  xor rax, rax
.loop:
  nop3
  inc rax
  cmp rax, rdx
  jb .loop
  ret

; ==============================================================================

nop3_all_bytes_asm:
  xor rax, rax
.loop:
  nop
  nop
  nop
  inc rax
  cmp rax, rdx
  jb .loop
  ret

; ==============================================================================

nop9_all_bytes_asm:
  xor rax, rax
.loop:
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  inc rax
  cmp rax, rdx
  jb .loop
  ret

; ==============================================================================

nop32_all_bytes_asm:
  xor rax, rax
.loop:
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  inc rax
  cmp rax, rdx
  jb .loop
  ret

