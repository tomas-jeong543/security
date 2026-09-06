[BITS 16]
[ORG 0x7C00]

; =========================================================
; 1st sector : Bootloader
; =========================================================

start:
    xor ax, ax
    mov ds, ax

    push es
    mov ax, 0xB800
    mov es, ax
    xor bx, bx
    mov bx, (5 * 80 ) * 2  
    
    mov [boot_drive], dl

    mov si, message
    ;여기서는 아직 sector2가 메모리로 올라오기 전이라 print_string_shell을 사용할 수 없다.
    call print_string

    pop es
    xor bx, bx

    ; -------------------------
    ; sector 2 -> 0x7E00
    ; -------------------------
    mov ax, 0x7C0
    mov es, ax
    mov bx, 0x0200

    mov ah, 0x02
    ;공간 부족 이슈로  sector를 1->2개로 늘림
    mov al, 0x04
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
    mov al,  [si]   
    
    cmp  al, 0
    je .done    
    
    mov byte [es:bx], al
    mov byte [es:bx+1], 0x07

    inc si
    add bx, 2
    jmp .print_loop

.done:
    ;pop es
    ;xor bx, bx
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
    jmp enter_protected_mode
    push es
    mov ax, 0xB800
    mov es, ax
    xor bx, bx
    ;이 부분도 하드 코딩이 아닌 변수로 바꿀 수 있으면 바꾸는 게 좋다.
    mov bx, (6 * 80 ) * 2

    mov si, message2
    call print_string_shell

    pop es
    xor bx, bx
    mov word [cursor_row], 8
    mov word [cursor_col], 0
   
; =========================================================
; 메인 Shell Loop
; =========================================================

shell_loop:

    ;mov bx, (8 * 80 ) * 2
    ; 프롬프트 출력
    
    
    mov si, introduce
    call print_string_shell
    
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

push ax
push bx    
;배열과 배열시작 인덱스의 초기화
mov di, input_char
xor cx, cx

.read_key:

    mov ah, 0x00
    int 0x16

    ; Enter?
    cmp al, 0x0D
    je .enter


    cmp al, 0x08
    je .backspace

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
    call put_char

    jmp .read_key

.enter:
    call newline
    jmp .finished
.finished:

    ; 문자열 끝
    mov byte [di], 0

    ;cmp cx, 0
    pop bx
    pop ax      
    ret
.backspace:
    ;제한 조건으로 만약 입력한 명령어가 없으면 계속 입력을 기다리게 함
    cmp cx, 0
    je .read_key
    ;문자열 길이와 현재 문자열 가리키는 포인터 주소를 감소시킨다.
    dec di
    dec cx
    ; backspace -> 공백 -> backspace순으로 출력해서 구현함 좀 더 구체적으로 말하면 현재 커서 위치에서 왼쪽으로 이동 후 공백을 출력한다.
    mov bx, [cursor_col]       
    dec bx
    
    mov [cursor_col], bx
    ;공백을 화면에 출력
    mov al, ' '
    call put_char
    ;커서를 한 칸 왼쪽으로 이동함 put_char에서 오른쪽으로 한 칸 이동시키기 때문이다.
    mov bx, [cursor_col]       
    dec bx
    mov [cursor_col], bx
   
    
    xor bx, bx
    call update_cursor
    jmp .read_key



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
   
    call print_string_shell
    call newline
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
    call print_string_shell
    call newline

    mov si, explain_command
    call print_string_shell
    call newline
    

    .loop:
        cmp cx, COMMAND_COUNT
        je .done

        mov si, [bx]
        call print_string_shell
        call newline    
        inc cx
        add bx, 4
        jmp .loop
    .done:
        call newline
        ret


do_clear:

    ; =====================================
    ; TODO:
    ; 화면 지우기
    ; =====================================
    ;값 오염 방지를 위한 push
    push ax
    push bx
    push es
    ;vga base주소와 offset를 각각 es와 bx로 초기화를 함
    mov ax, 0xB800
    mov es, ax
    xor bx, bx
    ;2000(80 * 25)번 동안 loop를 돌면서 화면에 전부 공백을 출력해서 비어 보이게 만든다
    .loop:
        cmp bx, 4000
        je .done

        mov [es:bx], ' '
        mov [es:bx + 1], 0x07
        add bx, 2
        jmp .loop
    ;화면을 지운 후에는 커서의 위치를 맨 위로 설정시켜 놓는ㄴ다.
    
    .done:
        mov [cursor_col], 0
        mov [cursor_row], 0
        call update_cursor
        pop es
        pop bx
        pop ax
        ret


; =========================================================
; 문자열
; =========================================================

message2:
    db 'Second sector!',  0

introduce:
    db 'Press Command: ', 0

accept:
    db 'Command detected!', 0

reject:
    db 'Unknown Command!',  0

; =========================================================
; 커서의 위치
; =========================================================
cursor_row:
     dw 8
cursor_col:
     dw 0

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
; 문자열 임시 저장 변수
; =========================================================
inschar:
    db 0
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


; =========================================================
; 문자 한 개를 출력하는 함수
; =========================================================
put_char:
    push ax
    push bx
    push es
    ; 입력:
    ; AL = 출력할 문자

    ; ---------------------------------
    ; 1. AL은 출력 문자이므로 보존할 필요가 있는지 고민
    ; ---------------------------------
    mov [inschar], al 

    ; ---------------------------------
    ; 2. cursor_row 가져오기
    ; ---------------------------------
    mov ax, [cursor_row]

    ; ---------------------------------
    ; 3. row * 80 계산
    ; ---------------------------------
    shl ax, 6
    mov bx, ax
    mov ax, [cursor_row]
    shl ax, 4
    add bx, ax
    ; ---------------------------------
    ; 4. cursor_col 더하기
    ; ---------------------------------
    add bx, [cursor_col]
    ; ---------------------------------
    ; 5. * 2
    ; 결과 = VGA offset
    ; ---------------------------------
    shl bx , 1

    ; ---------------------------------
    ; 6. ES = 0xB800 es와 같은 세그먼트 레지스터에는 immediate값을 바로 가져올 수 없다
    ; ---------------------------------
    mov ax, 0xB800
    mov es, ax
    ; ---------------------------------
    ; 7. 문자와 색상 기록
    ; [es:offset]   = 문자
    ; [es:offset+1] = 0x07
    ; ---------------------------------
    mov al, [inschar]
    mov byte [es:bx], al
    mov byte [es:bx+1], 0x07
    ; ---------------------------------
    ; 8. cursor_col++
    ; ---------------------------------
    mov ax, [cursor_col]
    cmp ax , 79
    je .change_row
    add ax, 1
    mov [cursor_col], ax
    jmp .done

.change_row:
    call newline

.done:
    call update_cursor
    pop es
    pop bx
    pop ax
    ret
print_string_shell:

.loop:
    mov al, [si]

    cmp al, 0
    je .done

    call put_char

    inc si
    jmp .loop

.done:
    ret
; ---------------------------------
; 커서를 다음 줄로 옮기는 함수로 이를 위해서 cursor_row +1 , cursor_col = 0으로 값을 변경함 -> 이후에는 기존처럼 finisehd함수를 출력한다. 
; ---------------------------------
newline:
    push bx
    push ax

    mov bx, [cursor_row]
    inc bx
    mov [cursor_row], bx
    mov word [cursor_col], 0

    cmp [cursor_row], 25
    je .done

    call update_cursor

    pop ax
    pop bx
    ret

    .done:

        call scroll_screen
        pop ax
        pop bx
        ret
; ---------------------------------
; 논리적 커서와 하드웨어적 커서를 일치시키는 함수로 커서의 위치가 변경되는 put_char ,newline, backspace,clear이 네 함수에서 실행이 된다. 
; ---------------------------------
update_cursor:
    push ax
    ;커서의 위치를 이동시키는 코드
    mov ax, [cursor_row]
    mov dh, al

    mov ax, [cursor_col]
    mov dl, al
    ;하드웨어 커서에 대한 기본 설정
    mov ah, 0x02
    mov bh, 0x00
    ;bios명령어 실행
    int 0x10
    
    pop ax
    ret
; ---------------------------------
; 화면이 가득찼을 때 모든 출력을 한 칸씩 올리는 스크롤 기능을 하는 함수다.
; ---------------------------------    
scroll_screen:
    ;사용할 레지스터 정보를 가져옴
    push ax
    push es
    push si
    push di
    push bx

    ; 1. ES = 0xB800
    mov ax, 0xB800
    mov es, ax
    mov bx,0


    ; 2. row 1 ~ row 24 내용을
    ;    row 0 ~ row 23으로 복사
    .loop:
        cmp bx, 3840
        je .done
        ;각 아랫 줄에서 문자와 그 색깔 정보를 위의 줄로 이동을 시키는 코드
        mov byte al ,  [es:bx + 160]
        mov byte [es:bx], al
        mov byte al ,  [es:bx + 161]
        mov byte [es:bx + 1], al
        ;다음 문자로 이동한다.
        add bx,2
        jmp .loop

    .done:
        ; 3. 마지막 row 24를
        ;    ' ' + 0x07로 채우기
        .end_loop:
            cmp bx, 4000
            je .end
            mov[es:bx], ' '
            mov[es: bx + 1], 0x07
            add bx, 2
            jmp .end_loop

        .end:            
            ; 4. cursor_row = 24
            ;    cursor_col = 0
            mov [cursor_row], 24
            mov [cursor_col], 0

            ; 5. update_cursor
            call update_cursor
            ;기존 레지스터 정보를 복구  
            pop bx
            pop di
            pop si
            pop es
            pop ax
            ret

; =========================
; Real mode에서 Protected Mode 전환하는 코드
; =========================
enter_protected_mode:

    ;cli는 Clear Interrupt Flag의 약자로 IF = 0으로 만들어서 Maskable Hardware Interrupt를 잠시 막는 명령어다.
    cli
    
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; 여기서는 아직 BITS 16
    jmp CODE_SEG:protected_mode_start

; =========================
; GDT 데이터
; =========================

gdt_start:
    ; null descriptor 사용하지 않는 디스크립터로 전부 다 0으로 채운다.
    dq 0
;여기서 gdt_code와 gdt_data의 값을 descriptor의 비트 구조에 맞게 8바이트를 다 채워야 한다.
gdt_code:
    ; code descriptor 8 bytes
    dq 0x00CF9A000000FFFF
gdt_data:
    ; data descriptor 8 bytes
    dq 0x00CF92000000FFFF
gdt_end:

;나중에 protected mode로 전환시 jmp CODE_SEG:protected_mode_start와 같이 쉽게 다른 영역에 접근하기 위해 selector를 미리 정의해놓음  
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

;CPU의 lgdt 명령어가 읽는 정보가 바로 이 구조로 굉장히 중요하다
gdt_descriptor:
    ;"GDT 전체 크기가 몇 바이트인가?"에 관한 정보를 2바이트로 저장하는 코드
    dw gdt_end - gdt_start - 1
    ;GDT가 메모리 어디에 있는가?"를 4바이트로 저장하는 코드
    dd gdt_start

[BITS 32]
protected_mode_start:

    mov ax, DATA_SEG

    mov ds, ax
    mov es, ax
    mov ss, ax
    
    ;Protected mode 진입 확인용 문자열 출력
    mov byte [0xB8900], 'K'
    mov byte [0xB8901], 0x0F
.hang:
    jmp .hang

times 2560-($-$$) db 0