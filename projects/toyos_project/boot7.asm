[BITS 16]
[ORG 0x7C00]

; =========================================================
; 1st sector : Bootloader
; =========================================================

start:
    xor ax, ax
    mov ds, ax

    mov [boot_drive], dl

    mov si, message
    call print_string

    ; -------------------------
    ; sector 2 -> 0x7E00
    ; -------------------------
    mov ax, 0x7C0
    mov es, ax
    mov bx, 0x0200

    mov ah, 0x02
    mov al, 0x01
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02
    mov dl, [boot_drive]

    int 0x13

    jmp 0x7E00


; =========================================================
; 공통 출력 함수
; SI = 출력할 문자열 주소
; =========================================================

print_string:

.print_loop:
    mov al, [si]

    cmp al, 0
    je .done

    mov ah, 0x0E
    int 0x10

    inc si
    jmp .print_loop

.done:
    ret


message:
    db 'Hello, OS!', 0

boot_drive:
    db 0


times 510-($-$$) db 0
dw 0xAA55


; =========================================================
; 2nd sector : Shell
; =========================================================

start2:

    mov si, message2
    call print_string


; =========================================================
; 메인 Shell Loop
; =========================================================

shell_loop:

    ; 프롬프트 출력
    mov si, introduce
    call print_string

    ; 사용자 입력
    call read_command

    ; 입력한 명령어 검색
    call find_command

    ; 다시 프롬프트로
    jmp shell_loop


; =========================================================
; 키보드 입력
;
; 결과:
; input_char = 입력 문자열
; =========================================================

read_command:

    mov di, input_char
    xor cx, cx

.read_key:

    mov ah, 0x00
    int 0x16

    ; Enter?
    cmp al, 0x0D
    je .finished


    ; =====================================
    ; TODO:
    ; 입력 길이가 31자를 넘지 않게 처리
    ;
    ; 힌트:
    ; CX에 현재 입력 길이를 저장
    ; =====================================
    cmp cx, 31
    je .finished

    mov [di], al
    inc di
    inc cx

    ; 입력한 문자 화면에 표시
    mov ah, 0x0E
    int 0x10

    jmp .read_key


.finished:

    ; 문자열 끝
    mov byte [di], 0

    ; 줄바꿈
    mov si, enter
    call print_string

    ret


; =========================================================
; 명령어 검색
;
; command_table 구조:
;
; +0  명령어 문자열 주소
; +2  처리 함수 주소
;
; 한 엔트리 = 4 byte
; =========================================================

find_command:

    mov bx, command_table
    mov cx, COMMAND_COUNT


.next_command:

    ; 입력 문자열
    mov si, input_char

    ; 현재 command 문자열
    mov di, [bx]

    call strcmp

    ; AL = 1이면 동일
    cmp al, 1
    je .found


    ; 다음 command entry
    add bx, 4

    loop .next_command


    ; =====================================================
    ; 아무 command도 발견하지 못함
    ; =====================================================

    mov si, reject
    call print_string

    ret


.found:

    ; BX + 2 = 해당 명령어 처리 함수 주소 호출
    call word [bx + 2]

    ret


; =========================================================
; strcmp
;
; SI = 사용자 입력
; DI = command 문자열
;
; return:
; AL = 1 : 동일
; AL = 0 : 다름
; =========================================================

strcmp:

.compare:

    mov al, [si]

    cmp al, [di]
    jne .different

    ; 둘 다 0까지 같으면 문자열 완전히 동일
    cmp al, 0
    je .same

    inc si
    inc di

    jmp .compare


.same:

    mov al, 1
    ret


.different:

    mov al, 0
    ret


; =========================================================
; Command handlers
; =========================================================

do_help:

    ; =====================================
    ; TODO:
    ; help 명령어 기능 구현
    ;
    ; 예:
    ; 사용 가능한 명령어 출력
    ; =====================================
    mov bx, command_table
    xor cx, cx
    
    mov si, accept
    call print_string
    
    mov si, explain_command
    call print_string
    
    

    .loop:
        cmp cx, COMMAND_COUNT
        je .done
        
        mov si, enter
        call print_string

        mov si, [bx]
        call print_string
        
        inc cx
        add bx, 4
        jmp .loop
    .done:
        ret


do_clear:

    ; =====================================
    ; TODO:
    ; 화면 지우기
    ;
    ; 힌트:
    ; INT 10h
    ; AH = 06h
    ; =====================================
    xor al, al
    mov ah, 0x06
    mov bh, 0x0F ;  0000 1111 배경과 글자 색으로 각각 검정과 밝은 흰색의 색깔을 나타낸다.
    mov cx, 0x0000      ; 왼쪽 위: row 0, col 0
    mov dx, 0x184F      ; 오른쪽 아래: row 24, col 79
    int 0x10
    ret


; =========================================================
; 문자열
; =========================================================

message2:
    db 'Second sector!', 0x0D, 0x0A, 0

introduce:
    db 0x0D, 0x0A, 'Press Command: ', 0

accept:
    db 'Command detected!', 0x0D, 0x0A, 0

reject:
    db 'Unknown Command!', 0x0D, 0x0A, 0

enter:
    db 0x0D, 0x0A, 0

explain_command:
    db ' -----The command that you can use------',0

; =========================================================
; 입력 Buffer
; =========================================================

input_char:
    times 32 db 0


; =========================================================
; Command 문자열
; =========================================================

cmd_help:
    db 'help', 0

cmd_clear:
    db 'clear', 0


; =========================================================
; Command Table
;
; 문자열 주소 + 함수 주소로 각각 word크기로 이루어져 있다.
; =========================================================

command_table:

    dw cmd_help
    dw do_help

    dw cmd_clear
    dw do_clear

command_table_end:

; 명령어 개수 자동 계산
COMMAND_COUNT equ (command_table_end - command_table) / 4

times 1024-($-$$) db 0