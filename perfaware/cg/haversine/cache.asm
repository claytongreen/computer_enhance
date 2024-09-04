section .text

global mov_32x4
global mov_32x6


; NOTE: rcx: count
; NOTE: rdx: data pointer
; NOTE:  r8: mask

mov_32x4:
  xor rax, rax ; NOTE: rax is non-clamped offset
  align 64
.loop:
  mov r10, rax ; NOTE: move offset to r10
  and r10, r8  ; NOTE: mask offset
  add r10, rdx ; NOTE: offset from data pointer
  vmovdqu ymm0, [r10 +  0] ; [ 0 -  32)
  vmovdqu ymm0, [r10 + 32] ; [32 -  64)
  vmovdqu ymm0, [r10 + 64] ; [64 -  96)
  vmovdqu ymm0, [r10 + 96] ; [96 - 128)
  add rax, 128 ; 32*4
  cmp rax, rcx
  jb .loop
  ret

mov_32x6:
  xor rax, rax
  align 64
.loop:
  mov r10, rax ; NOTE: move offset to r10
  and r10, r8  ; NOTE: mask offset
  add r10, rdx ; NOTE: offset from data pointer
  vmovdqu ymm0, [r10 +   0]
  vmovdqu ymm0, [r10 +  32]
  vmovdqu ymm0, [r10 +  64]
  vmovdqu ymm0, [r10 +  96]
  vmovdqu ymm0, [r10 + 128]
  vmovdqu ymm0, [r10 + 160]
  add rax, 192
  cmp rax, rcx
  jb .loop
  ret

