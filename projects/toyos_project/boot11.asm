[BITS 16]
[ORG 0x7C00]

; =========================================================
; 1st sector : Bootloader
; =========================================================

start:
    xor ax, ax
    mov ds, ax

    mov [boot_drive], dl

    ; 부팅 확인용 출력
    mov ax, 0xB800
    mov es, ax
    mov bx, (5 * 80) * 2

    mov si, message
    call print_string_real

    ; -----------------------------------------------------
    ; 추가 4 sector -> 0x7E00
    ; -----------------------------------------------------

    mov ax, 0x07C0
    mov es, ax
    mov bx, 0x0200

    mov ah, 0x02
    mov al, 0x04
    mov ch, 0
    mov dh, 0
    mov cl, 2
    mov dl, [boot_drive]

    int 0x13

    jmp 0x7E00


; =========================================================
; Real Mode 전용 문자열 출력
; Protected Mode 전환 후에는 사용하지 않는다.
; =========================================================

print_string_real:

.loop:
    mov al, [si]

    cmp al, 0
    je .done

    mov byte [es:bx], al
    mov byte [es:bx + 1], 0x07

    inc si
    add bx, 2

    jmp .loop

.done:
    ret


message:
    db 'Hello, OS!', 0

boot_drive:
    db 0


times 510 - ($ - $$) db 0
dw 0xAA55


; =========================================================
; Protected Mode 진입
; =========================================================

start2:
    jmp enter_protected_mode


enter_protected_mode:

    cli

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:protected_mode_start


; =========================================================
; GDT
; =========================================================

gdt_start:

gdt_null:
    dq 0

gdt_code:
    dq 0x00CF9A000000FFFF

gdt_data:
    dq 0x00CF92000000FFFF

gdt_end:


CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start


gdt_descriptor:
    
    dw gdt_end - gdt_start - 1
    ;시작 주소
    dd gdt_start


; =========================================================
; 여기부터 Protected Mode
; =========================================================
 
[BITS 32]


protected_mode_start:
    STACK_TOP equ 0x90000
    ; -----------------------------------------------------
    ; Segment register 초기화
    ; -----------------------------------------------------

    mov ax, DATA_SEG

    mov ds, ax
    mov es, ax
    mov ss, ax

    ; -----------------------------------------------------
    ; TODO 0
    ; ESP를 안전한 위치로 초기화할 것.
    ; Protected Mode에서 call / push를 계속 사용할 예정이므로
    ; stack pointer를 명시적으로 설정해야 한다.
    ; -----------------------------------------------------
    mov esp, STACK_TOP

    ; -----------------------------------------------------
    ; TODO5까지 잘 됬는지 확인하는 코드
    ; -----------------------------------------------------

    call clear_screen32

    mov esi, message2
    call print_string32

    call newline32

    mov esi, introduce
    call print_string32    
    
    ; todo6확인 코드
    ;call do_help32

    ;todo7검증 코드
    mov esi, test_command
    call find_command32
    
    mov esi, test_command2
    call find_command32
    
    mov esi, test_command3
    call find_command32

.hang:
    jmp .hang



; =========================================================
; Protected Mode에서 사용할 데이터
; =========================================================
;todo7 테스트용
test_command:
    db 'help', 0
test_command2:
    db 'clear', 0
test_command3:
    db 'hope', 0

;idt table공간  우선은 0으로 초기화를 함
idt_start:
    times 256 dq 0
idt_end:    

cursor_row:
    dd 1

cursor_col:
    dd 0


inschar:
    db 0

message2:
    db 'Protected Mode!', 0

introduce:
    db 'Press Command: ', 0

accept:
    db 'Command detected!', 0

reject:
    db 'Unknown Command!', 0

explain_command:
    db '-----The command that you can use-----', 0


input_char:
    times 32 db 0


cmd_help:
    db 'help', 0

cmd_clear:
    db 'clear', 0



; =========================================================
; TODO 1 : put_char32
; 기존 put_char를 Protected Mode용으로 변환
; 입력:
; AL = 출력할 문자
; =========================================================

put_char32:
    push eax
    push ebx
     
    ; -----------------------------------------------------
    ; TODO 1-1
    ; AL 값을 보존할 것.
    mov [inschar], al

    ; -----------------------------------------------------
    ; TODO 1-2
    ; offset = ((cursor_row * 80) + cursor_col) * 2
    ; -----------------------------------------------------
    ;offset = cursor_row * 80
    mov ebx, [cursor_row]
    shl ebx, 6
    mov eax, [cursor_row]
    shl eax, 4
    add ebx, eax
    ; + cursor_col 
    mov eax, [cursor_col]
    add ebx, eax
    ; *2
    shl ebx, 1

    ; -----------------------------------------------------
    ; TODO 1-3
    ; 문자와 속성값 0x07을 VGA memory에 기록
    ; -----------------------------------------------------
    mov al, [inschar]
    mov byte [0xB8000 + ebx], al
    mov byte [0xB8000 + ebx + 1], 0x07
    ; -----------------------------------------------------
    ; TODO 1-4
    ; cursor_col 증가
    ; col == 79인 경우 newline32 호출
    ; -----------------------------------------------------
    mov eax, [cursor_col]
    inc eax
    mov [cursor_col], eax

    cmp dword [cursor_col], 80
    je .newline

    pop ebx
    pop eax
    ret

    .newline:
        call newline32
 
    pop ebx
    pop eax
    ret

; =========================================================
; TODO 2 : print_string32
;
; 기존 print_string_shell의 구조를 거의 그대로 사용 가능
;
; ESI = 문자열 주소
; =========================================================

print_string32:

push eax
push esi

.loop:
    ; TODO:
    ; [esi]에서 문자 하나 읽기
    ; 0이면 .done  아니면 put_char32 호출
    ; esi++ ,loop 반복
    mov al, [esi]

    cmp al, 0
    je .done

    call put_char32
    inc esi
    jmp .loop
.done:
    pop esi
    pop eax
    ret



; =========================================================
; TODO 3 : newline32
;
; 기존 newline 로직을 거의 그대로 사용
; =========================================================

newline32:

    ; TODO: cursor_row++ , cursor_col = 0
    ; cursor_row == 25 라면 scroll_screen32 호출

    push eax

    mov eax, [cursor_row]
    inc eax
    mov [cursor_row], eax

    mov [cursor_col], 0

    cmp eax , 25
    je .scroll_screen
    pop eax
    ret

.scroll_screen:
    call scroll_screen32

    pop eax
    ret



; =========================================================
; TODO 4 : scroll_screen32
;
; 기존 scroll_screen 알고리즘 재사용
; =========================================================

scroll_screen32:
    
    push ebx
    push eax
    ; TODO:
    ;
    ; EBX = 0
    ;
    ; EBX < 3840 동안
    ;
    ; [0xB8000 + ebx + 160]
    ;       ↓
    ; [0xB8000 + ebx]
    ;
    ; 문자와 attribute 모두 복사
    mov ebx, 0
    .loop:
        cmp ebx, 3840
        je .last_line
        ;문자와 그 색깔 값을 저장
        mov eax, [0xB8000 + ebx + 160]
        mov [0xB8000 + ebx], eax 
        add ebx, 4
        jmp .loop

    .last_line:
    ; TODO: 마지막 160 byte를  ' ' + 0x07로 채우기
        ;마지막 줄의 마지막 행에 도달시 처리
        cmp ebx, 4000
        je .done

        mov byte [0xB8000 + ebx], ' '
        mov byte [0xB8000 + ebx + 1], 0x07
        add ebx, 2
        jmp .last_line

    ; TODO:
    ;cursor_row = 24,  cursor_col = 0
    .done:
        mov dword [cursor_row], 24
        mov dword [cursor_col], 0
        
        pop eax
        pop ebx
        ret


; =========================================================
; TODO 5 : clear_screen32
;
; 기존 do_clear의 화면 지우기 로직 재사용
; =========================================================

clear_screen32:

    ; TODO: VGA memory 4000 byte를  문자 = ' ' attribute = 0x07로 채운다. 그 후 커서를 0,0에 위치시키기

    push ebx
    push eax

    mov ebx, 0

    .loop:

        cmp ebx, 4000
        je .done
        
        mov [0xB8000 + ebx ], ' '
        mov [0xB8000 + ebx + 1], 0x07
        
        add ebx , 2
        jmp .loop

    .done:
        mov dword [cursor_row], 0
        mov dword [cursor_col], 0

    pop eax
    pop ebx

    ret



; =========================================================
; strcmp
;
; 이 함수의 핵심 알고리즘은 그대로 사용할 수 있다.
;
; ESI = 문자열 1
; EDI = 문자열 2
;
; return:
; AL = 1 : 동일
; AL = 0 : 다름
; =========================================================

strcmp32:

.compare:

    mov al, [esi]

    cmp al, [edi]
    jne .different

    cmp al, 0
    je .same

    inc esi
    inc edi

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

do_help32:

    ; -----------------------------------------------------
    ; TODO -6
    ;print_string32가 완성된 다음 기존 do_help를
    ; 32bit command table 형식에 맞춰 변환한다.
    ; -----------------------------------------------------
    push esi
    push ebx

    ;줄바꿈    
    call newline32
    ;문자열 출력    
    mov esi, explain_command
    call print_string32
    ;줄바꿈 후  help 명령어 문자열로 호출
    call newline32
    mov esi, cmd_help
    call print_string32
    
    ; 줄 바꿈 후 clear함수 호출
    call newline32
    mov esi, cmd_clear
    call print_string32
    call newline32

    pop ebx
    pop esi    
    ret



do_clear32:

    call clear_screen32
    ret



; =========================================================
; Command Table
;
; Protected Mode에서는 주소를 32bit로 사용할 것이므로
;
; 기존:
;
;     dw cmd_help
;     dw do_help
;
; 대신 dd를 사용한다.
;
; 한 entry = 8 byte
; =========================================================

command_table:

    dd cmd_help
    dd do_help32

    dd cmd_clear
    dd do_clear32


command_table_end:


COMMAND_COUNT equ (command_table_end - command_table) / 8



; =========================================================
; TODO-7
;
; find_command32
;
; print_string32와 command handler가 정상 동작한 뒤 구현.
; =========================================================

find_command32:

    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    ;사용자가 입력한 문자열 주소를 저장함
    mov edx,esi

    ;명령어의 테이블 주소와 명령어 개수를 레지스터에 저장함 
    mov ebx, command_table
    mov ecx, COMMAND_COUNT

.loop:
    cmp ecx, 0
    je .not_found
    ;사용자가 입력한 문자열의 주소를 다시 가져옴
    mov esi, edx
    mov edi, [ebx]
    
    call strcmp32
    ;명령어 발견시 found로 이동
    cmp al, 1
    je .found
    ;다음 명령어 주소로 이동 dd 2개가 명령어 당 차지하기 때문에 +8을 한다.
    add ebx, 8
    dec ecx
    jmp .loop

.found:
    call dword [ebx + 4]
    jmp .done    

.not_found:
    mov esi, reject
    call print_string32
    call newline32
    
.done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; =========================================================
; TODO - 훨씬 나중에 구현
;
; read_command32
;
; 기존 BIOS:
;
;     int 0x16
;
; 은 Protected Mode에서 사용하지 않는다.
;
; 나중에:
;
; IDT
; PIC
; IRQ1
; keyboard port 0x60
;
; 를 배운 다음 구현한다.
; =========================================================

read_command32:

    ; TODO:
    ; 아직 구현하지 말 것.

    ret



; =========================================================
; TODO - keyboard 입력까지 구현한 뒤 활성화
; =========================================================

shell_loop32:

    ; TODO:
    ;
    ; introduce 출력
    ;
    ; read_command32
    ;
    ; find_command32
    ;
    ; 다시 반복

    ret

times 2560 - ($ - $$) db 0