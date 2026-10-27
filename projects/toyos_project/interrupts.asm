;각 코드에 대한 해석이 필요하다.
[BITS 32]

global init_interrupts
global keyboard_irq_stub

extern keyboard_handler

CODE_SEG equ 0x08

; =========================================================
; C에서 호출하는 전체 interrupt 초기화 함수
; =========================================================
init_interrupts:
    call init_idt

    lidt [idt_descriptor]

    call remap_pic
    call mask_pic

    sti
    ret


; =========================================================
; Keyboard IRQ1 stub
; 실제 처리는 C의 keyboard_handler()에게 넘김
; =========================================================
keyboard_irq_stub:
    pushad

    call keyboard_handler

    popad
    iretd


; =========================================================
; IDT 초기화
; IRQ1 -> vector 0x21
; =========================================================
init_idt:
    push esi
    push edi

    mov esi, keyboard_irq_stub
    mov edi, idt_start + 0x21 * 8
    call set_idt

    pop edi
    pop esi
    ret


; =========================================================
; IDT entry 1개 설정
;
; ESI = handler address
; EDI = IDT entry address
; =========================================================
set_idt:
    push eax

    mov eax, esi

    ; handler offset 하위 16bit
    mov word [edi], ax

    ; kernel code segment selector
    mov word [edi + 2], CODE_SEG

    ; reserved
    mov byte [edi + 4], 0

    ; present=1, DPL=0, 32-bit interrupt gate
    mov byte [edi + 5], 0x8E

    ; handler offset 상위 16bit
    shr eax, 16
    mov word [edi + 6], ax

    pop eax
    ret


; =========================================================
; PIC remap
;
; Master PIC : IRQ0~7  -> 0x20~0x27
; Slave PIC  : IRQ8~15 -> 0x28~0x2F
; =========================================================
remap_pic:
    ; ICW1
    mov al, 0x11
    out 0x20, al

    mov al, 0x11
    out 0xA0, al

    ; ICW2
    mov al, 0x20
    out 0x21, al

    mov al, 0x28
    out 0xA1, al

    ; ICW3
    mov al, 0x04
    out 0x21, al

    mov al, 0x02
    out 0xA1, al

    ; ICW4
    mov al, 0x01
    out 0x21, al

    mov al, 0x01
    out 0xA1, al

    ret


; =========================================================
; IRQ1(keyboard)만 허용
; =========================================================
mask_pic:
    ; 11111101
    ; IRQ1 bit만 0 -> 허용
    mov al, 0xFD
    out 0x21, al

    ; Slave PIC 전부 차단
    mov al, 0xFF
    out 0xA1, al

    ret
; =========================================================
; IDT descriptor / table
; =========================================================
idt_descriptor:
    dw idt_end - idt_start - 1
    dd idt_start

idt_start:
    times 256 dq 0
idt_end: