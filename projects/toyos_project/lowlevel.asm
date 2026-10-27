[BITS 32]

global inb
global outb

;하드웨어에서 i/o 입력을 1바이트 읽는 함수
inb:
    ; TODO:
    ; port는 unsigned short → 2바이트
    mov dx, word[esp + 4]
    
    ; 지정된 I/O port에서 1바이트 읽기
    in al, dx
    ;zero extention
    movzx eax, al

    ret
;하드웨어에서 i/o출력을 1바이트 내보내는 함수    

outb:
    ; TODO:
    ; [esp+4]에서 port 가져오기
    mov dx, word[esp + 4]

    ; [esp+8]에서 value 가져오기
    mov al, byte[esp + 8]
    
    ; dx / al에 넣기
    out dx, al
    
    ret


