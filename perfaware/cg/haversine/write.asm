section .text

global write_x1
global write_x2
global write_x3
global write_x4
global write_x5
global write_x8

; rcx: count
; rdx: data

write_x1:
  align 64
.loop:
  mov [rdx], rcx
  sub rcx, 1
  jnle .loop
  ret

write_x2:
  align 64
.loop:
  mov [rdx], rcx
  mov [rdx], rcx
  sub rcx, 2
  jnle .loop
  ret

write_x3:
  align 64
.loop:
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  sub rcx, 3
  jnle .loop
  ret

write_x4:
  align 64
.loop:
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  sub rcx, 4
  jnle .loop
  ret

write_x5:
  align 64
.loop:
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  sub rcx, 5
  jnle .loop
  ret

write_x8:
  align 64
.loop:
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  mov [rdx], rcx
  sub rcx, 8
  jnle .loop
  ret
