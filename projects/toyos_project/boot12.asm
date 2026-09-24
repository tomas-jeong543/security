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
    ; 추가 63 sector 
    ; -----------------------------------------------------

    mov ax, 0x07C0
    mov es, ax
    mov bx, 0x0200

    mov ah, 0x02
    mov al, 0x3F
    mov ch, 0
    mov dh, 0
    mov cl, 2
    mov dl, [boot_drive]

    int 0x13

    ; kernel.bin -> 0x10000 로드
    ;SI에 disk_address_packet의 주소를 넣는 명령어
    mov si, disk_address_packet
    ;LBA기반의 읽기
    mov ah, 0x42
    ;어떤 디스크에서 읽어올지를 결정
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

; kernel.bin을 읽기 위한 Disk Address Packet
disk_address_packet:
    db 0x10          ; DAP 크기 = 16 bytes
    db 0x00          ; reserved
    dw 0x0001        ; 읽을 sector 수 = 1

    dw 0x0000        ; destination offset
    dw 0x1000        ; destination segment
                     ; 0x1000:0x0000 = 물리주소 0x10000

    dq 64            ; kernel.bin이 있는 LBA 64

times 510 - ($ - $$) db 0
dw 0xAA55


; =========================================================
; Protected Mode 진입
; =========================================================

start2:
    jmp enter_protected_mode


enter_protected_mode:

    cli

    ;gdt, idt 관련 정보를 초기화를 함
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
;kernel_entry.asm이랑 kernel.c를 컴파일해서 생길 kernel.bin을 올릴 주소
KERNEL_LOAD_ADDR equ 0x10000

gdt_descriptor:
    
    dw gdt_end - gdt_start - 1
    ;시작 주소
    dd gdt_start


; =========================================================
; 여기부터 Protected Mode
; =========================================================
 
[BITS 32]

; =========================================================
; IDT 관련 데이터 및 함수들
; =========================================================

;idt table공간  우선은 0으로 초기화를 함
idt_start:
    times 256 dq 0
idt_end:    

idt_descriptor:
    ;idt 개수 정보
    dw idt_end - idt_start -1
    ;시작 주소
    dd idt_start

;test를 위한 핸들러
test_interrupt_handler:
    push esi

    mov esi, test_command4    
    call print_string32
    pop esi
    iretd

keyboard_interrupt_handler:

    ; 필요한 register 저장
    push esi
    push eax
    push ebx

    ;데이터 포트에서 값을 받아옴
    in al, 0x60
    test al, 0x80
    
    ;키보드를 뗏을 때 처리 과정
    jnz .send_eoi

    ;아스키 코드로 변환
    movzx eax, al
    ;table 범위를 넘어갔을 때에 대한 처리
    cmp eax, Scan_COUNT
    jae .send_eoi
    
    mov al, [Scan_table + eax]
    
    ;지원하지 않는 문자열인 경우에 대한 처리
    cmp al, 0
    je .send_eoi
    ;backspace key처리
    cmp al, 0x08    
    je .backspace

    ;enter key 처리
    cmp al, 0x0D
    je .enter
    ;키보드 문자열 처리
    mov ebx, [input_index]

    cmp ebx, 31
    jae .send_eoi

    mov [input_char + ebx], al
    inc dword [input_index]

    call put_char32
    jmp .send_eoi




    .backspace:
        cmp dword [input_index], 0
        je .send_eoi

        dec  dword [input_index]
        mov ebx, [input_index]
        mov byte [input_char + ebx], 0

        call backspace32
        jmp .send_eoi
    
    .enter:
        mov eax, [input_index]
        mov  byte [input_char + eax], 0
        call newline32
        mov byte  [command_ready], 1
        jmp .send_eoi

    .send_eoi:
    
        ; PIC에게 interrupt 처리 끝났다고 알림
        mov al, 0x20
        out 0x20, al
        
        ; register 복구
        pop ebx
        pop eax
        pop esi
        
        ;종료
        iretd 

;범용 핸들러
init_idt:
    push esi
    push edi
    
    ;test 핸들러를 등록
    mov esi,test_interrupt_handler
    mov edi,idt_start + 0x30 * 8
    call set_idt

    ;keybord handler등록
    mov esi,keyboard_interrupt_handler
    mov edi, idt_start + 0x21 * 8    
    call set_idt

    pop edi
    pop esi
    ret

;실제 idt table의 8바이트를 주어진 레지스터의 정보를 통해서 채우는 함수
set_idt:
    push  eax

    mov eax, esi
    mov word [edi], ax
    mov word [edi + 2], CODE_SEG
    mov byte [edi + 4], 0
    mov byte [edi + 5], 0x8E
    
    ;상위 16비트를 이동시킴 shr로 shift연산으로 이동시킴
    shr eax, 16
    mov word [edi + 6],ax 
    
    pop eax
    ret


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

    jmp KERNEL_LOAD_ADDR

    call init_idt

    lidt [idt_descriptor]
    call remap_pic
    
    ; 마스킹 후 hardware irq를 받게 sti를 설정함
    call mask_pic

    call clear_screen32
    sti    
    
    ;셸 진입
    call shell_loop32


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
;todo8용
test_command4:
    db 'Hello', 0
;todo10용
test_command5:
    db'k',0

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
input_index:
    dd 0

;enter key가 입력됬음을 나타내는 플래그
command_ready:
    db 0

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

    .update:
        call update_cursor32
        pop ebx
        pop eax
        ret

    .newline:
        call newline32
        jmp .update

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
; TODO 13 : backspace함수  -> protected mode keyboard handler용
; =========================================================
backspace32:
    push eax    
    push ebx
    mov eax, [cursor_col]
    cmp eax, 0
    je .done

    dec eax
    mov [cursor_col], eax
    
    ; offset = ((cursor_row * 80) + cursor_col) * 2
    mov eax, [cursor_row]
    mov ebx, [cursor_row]

    shl ebx, 6
    shl eax, 4
    add ebx, eax

    mov eax, [cursor_col]
    add ebx, eax
    shl ebx, 1

    ;공백 집어넣기
    mov byte [0xB8000 + ebx], ' '
    mov byte [0xB8000 + ebx + 1], 0x07    
    
    .done:
        pop ebx
        pop eax
        ret

; =========================================================
; TODO 3 : newline32
; 기존 newline 로직을 거의 그대로 사용
; =========================================================

newline32:

    ; TODO: cursor_row++ , cursor_col = 0
    ; cursor_row == 25 라면 scroll_screen32 호출

    push eax

    mov eax, [cursor_row]
    inc eax
    mov [cursor_row], eax

    mov dword [cursor_col], 0

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
        
        mov byte [0xB8000 + ebx ], ' '
        mov byte [0xB8000 + ebx + 1], 0x07
        
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
;여기에 빠진 것부터 수정해야 함
Scan_table:

    db 0
    db 0x1B
    db 0x31
    db 0x32
    db 0x33
    db 0x34
    db 0x35
    db 0x36
    db 0x37
    db 0x38
    db 0x39
    db 0x30
    db 0x2D
    db 0x3D
    db 0x08
    db 0x09
    db 0x71
    db 0x77
    db 0x65
    db 0x72
    db 0x74
    db 0x79
    db 0x75
    db 0x69
    db 0x6F
    db 0x70
    db 0x5B
    db 0x5D
    db 0x0D
    db 0   ; 0x1D Left Ctrl
    
    db 0x61
    db 0x73
    db 0x64
    db 0x66
    db 0x67
    db 0x68
    db 0x6A
    db 0x6B
    db 0x6C
    db 0x3B
    db 0x27
    db 0x60
    db 0   ; 0x2A Left Shift 

    db 0x5C
    db 0x7A
    db 0x78
    db 0x63
    db 0x76
    db 0x62
    db 0x6E
    db 0x6D
    db 0x2C
    db 0x2E
    db 0x2F
    
    db 0 ; 0x36 Right Shift 
    db 0 ; 0x37 Keypad * 
    db 0 ; 0x38 Alt 
    db 0x20 ; space

Scan_table_end:


Scan_COUNT equ (Scan_table_end - Scan_table)

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

remap_pic:
    ;PIC를 초기화하고 ICW4도 뒤에 보낼 거라는 의미다.
    mov al, 0x11
    out 0x20, al

    mov al, 0x11
    out 0xA0, al                    
    ;시작 위치 설정(인터럽트 핸들러 기준)
    mov al, 0x20
    out 0x21, al

    mov al, 0x28
    out 0xA1, al

    ;master와  slave와의 관계를 전달
    ;master에 0x04인 이유는 0000 0100 즉 irq2에 slave가 연결되어 있음을 알리기 위함
    mov al, 0x04
    out 0x21, al

    ;slave가 0x02인 이유는 master의 irq2에 연결이 되어 있기 때문
    mov al, 0x02
    out 0xA1, al

    ;8086/88 mode를 설정 양쪽의 PIC의 데이터 부분에 0x01보냄
    mov al, 0x01

    out 0x21, al
    out 0xA1, al
    ret

mask_pic:
    ; Master는 IRQ1을 제외하고 전부 마스킹을 해서 차단함    
    mov al, 0xFD
    out 0x21, al
    
    ;Slave는 우선은 전부 마스킹을 해서 차단함
    mov al, 0xFF
    out 0xA1, al

    ret

;하드웨어 커서와 물리 커서를 일치시키는 함수
update_cursor32:
    push eax
    push ebx
    push edx
    
    ;cursor_row * 80 + cursor_col
    mov eax, [cursor_row]
    mov ebx, [cursor_row]
    shl eax, 6
    shl ebx, 4
    add eax, ebx

    mov ebx, [cursor_col]
    add eax, ebx

    mov bx,ax

    ;VGA index port를 선택
    mov dx, 0x3D4

    ;그다음 0x0E를 보내서 이제 Cursor High 레지스터를 수정할 거라는 것을 알림
    mov al, 0x0E
    out dx, al
    
    ;데이터 포트 선택 후 상위 8비트를 보냄
    mov dx, 0x3D5
    mov al, bh
    out dx, al    

     ;VGA index port를 선택
    mov dx, 0x3D4
    ;그다음 0x0F를 보내서 이제 Cursor Low 레지스터를 수정할 거라는 것을 알림
    mov al, 0x0F
    out dx, al
    ;데이터 포트 선택 후 하위 8비트를 보냄
    mov dx, 0x3D5
    mov al, bl
    out dx, al    

    pop edx
    pop ebx
    pop eax

    ret


; =========================================================
; TODO - keyboard 입력까지 구현한 뒤 활성화
; =========================================================

shell_loop32:
    ;명령 입력 안내 출력
    mov esi, introduce
    call print_string32
    
    .wait:
        cmp byte [command_ready], 0
        je .wait
        
        ;입력 받은 명령어를 찾으려 함
        mov esi, input_char
        call find_command32
        
        ;다시 입력 버퍼를 초기화
        mov dword [input_index], 0
        mov byte [command_ready], 0
        mov byte [input_char], 0

        ;명령 입력 안내 출력
        mov esi, introduce
        call print_string32

        jmp .wait

times 32768 - ($ - $$) db 0