extern main
extern exit
extern __init_libj6
extern __init_libc

global _start:function weak (_libc_crt0_start.end - _libc_crt0_start)
global _libc_crt0_start:function (_libc_crt0_start.end - _libc_crt0_start)

_start:
_libc_crt0_start:
    push 0              ; Add null frame
    push 0
    mov rbp, rsp

    call __init_libj6
    mov rbx, rax

    call __init_libc

    pop rdi
    mov rsi, rsp
    mov rdx, 0    ; TODO: actually parse stack for argc, argv, envp
    mov rcx, rbx
    call main

    mov rdi, rax
    call exit
.end:
