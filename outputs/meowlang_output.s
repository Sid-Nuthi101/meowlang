.align 2
_meowfn_add:
    sub sp, sp, #96
    str x30, [sp, #80]
    str x0, [sp, #0]
    str x1, [sp, #8]
    ldr x9, [sp, #0]
    ldr x10, [sp, #8]
    add x9, x9, x10
    str x9, [sp, #16]
    ldr x9, [sp, #16]
    mov x0, x9
    ldr x30, [sp, #80]
    add sp, sp, #96
    ret
.global _main
.align 2
_main:
    sub sp, sp, #80
    str x30, [sp, #64]
    mov x9, #1
    mov x10, #2
    mov x11, #3
    mov x0, x10
    mov x1, x11
    str x9, [sp, #8]
    bl _meowfn_add
    ldr x9, [sp, #8]
    mov x10, x0
    mov x0, x9
    mov x1, x10
    bl _meowfn_add
    mov x9, x0
    str x9, [sp, #0]
    mov x0, x9
    ldr x30, [sp, #64]
    add sp, sp, #80
    ret
