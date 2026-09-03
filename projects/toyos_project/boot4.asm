[BITS 16]
[ORG 0x7C00]

start:
    mov si, message
    xor ax, ax
    mov ds, ax
    mov [boot_drive], dl
print_loop:
    
    mov al, [si]
    cmp al, 0
    je done    
    mov ah, 0x0E
    int 0x10
    inc si
    jmp print_loop
message:
    db 'Hello, OS!', 0
boot_drive:
    db 0    
done:
    mov ax, 0x7C0
    mov es, ax
    mov bx, 0x0200
    
    mov ah, 0x02
    
    mov ch, 0x00   
    mov dh, 0x00    
    mov cl, 0x02    
    mov al, 0x01

    mov dl, [boot_drive]

    int 0x13
    jmp 0x7E00
times 510-($-$$) db 0
dw 0xAA55

start2:
    mov si, message2
    xor cx, cx
print_loop2:
    mov al, [si]
    cmp al, 0
    je brabch    
    mov ah, 0x0E
    int 0x10
    inc si
    jmp print_loop2
key_get:
    mov cx, 1
    mov si, introduce
    jmp print_loop2
key_print:
    mov cx, 2    
    mov ah, 0x00
    int 0x16
   
    mov [input_char], al
    
    mov ah, 0x0E
    int 0x10
   
    mov si, explanation
    jmp print_loop2
done2:
    mov al, [input_char]
    mov ah, 0x0E
    int 0x10
    jmp loop 
brabch:
    cmp  cx,0 
    je key_get
    cmp cx, 1
    je key_print
    cmp cx, 2
    je done2
loop:
    jmp loop    

message2:
    db 'Second sector!', 0x0D, 0x0A, 0    
introduce:
    db 0x0D, 0x0A,'Press a Key:', 0
explanation:        
    db 0x0D, 0x0A,'You pressed ', 0
input_char:
    db 0    

times 1024-($-$$) db 0