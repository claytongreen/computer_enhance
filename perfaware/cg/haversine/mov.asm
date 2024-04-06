section .text

global mov_4x2
global mov_4x3
global mov_8x2
global mov_8x3
global mov_16x2
global mov_16x3
global mov_32x2
global mov_32x3
global mov_64x2
global mov_64x3

; rcx: count
; rdx: data

mov_4x2:
  xor rax, rax
  align 64
.loop:
  mov r8d, [rdx + 0]
  mov r9d, [rdx + 4]
  add rax, 8
  cmp rax, rcx
  jb .loop
  ret

mov_4x3:
  xor rax, rax
  align 64
.loop:
  mov r8d, [rdx + 0]
  mov r9d, [rdx + 4]
  ; add rax, 8
  mov r10d, [rdx + 8]
  add rax, 12
  cmp rax, rcx
  jb .loop
  ret

mov_8x2:
  xor rax, rax
  align 64
.loop:
  mov r8, [rdx + 0]
  mov r9, [rdx + 8]
  add rax, 16
  cmp rax, rcx
  jb .loop
  ret

mov_8x3:
  xor rax, rax
  align 64
.loop:
  mov r8, [rdx + 0]
  mov r9, [rdx + 8]
  ; add rax, 16
  mov r10, [rdx + 16]
  add rax, 24
  cmp rax, rcx
  jb .loop
  ret

mov_16x2:
  xor rax, rax
  align 64
.loop:
  vmovdqu xmm0, [rdx +  0]
  vmovdqu xmm1, [rdx + 16]
  add rax, 32
  cmp rax, rcx
  jb .loop
  ret

mov_16x3:
  xor rax, rax
  align 64
.loop:
  vmovdqu xmm0, [rdx +  0]
  vmovdqu xmm1, [rdx + 16]
  ; add rax, 32
  vmovdqu xmm1, [rdx + 32]
  add rax, 48
  cmp rax, rcx
  jb .loop
  ret

mov_32x2:
  xor rax, rax
  align 64
.loop:
  vmovdqu ymm0, [rdx +  0]
  vmovdqu ymm1, [rdx + 32]
  add rax, 64
  cmp rax, rcx
  jb .loop
  ret

mov_32x3:
  xor rax, rax
  align 64
.loop:
  vmovdqu ymm0, [rdx +  0]
  vmovdqu ymm1, [rdx + 32]
  ; add rax, 64
  vmovdqu ymm1, [rdx + 64]
  add rax, 96
  cmp rax, rcx
  jb .loop
  ret

mov_64x2:
  xor rax, rax
  align 64
.loop:
  vmovdqu64 zmm0, [rdx +   0]
  vmovdqu64 zmm1, [rdx +  64]
  add rax, 128
  cmp rax, rcx
  jb .loop
  ret

mov_64x3:
  xor rax, rax
  align 64
.loop:
  vmovdqu64 zmm0, [rdx +   0]
  vmovdqu64 zmm1, [rdx +  64]
  ; add rax, 128
  vmovdqu64 zmm1, [rdx + 128]
  add rax, 192
  cmp rax, rcx
  jb .loop
  ret

