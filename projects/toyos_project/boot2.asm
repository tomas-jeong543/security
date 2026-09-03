[BITS 16]
[ORG 0x7C00]

start:
    mov si, message
    xor ax, ax
    mov ds, ax
print_loop:
    
    mov al, [si]
    cmp al, 0
    je done    
    mov ah, 0x0E
    int 0x10
    inc si
    jmp print_loop
hang:
    jmp hang

message:
    db 'Hello, OS!', 0
done:
    jmp hang


times 510-($-$$) db 0
dw 0xAA55