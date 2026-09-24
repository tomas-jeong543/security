; ============================================================
; kernel_entry.asm
;
; 역할:
;   boot11.asm으로부터 Protected Mode 상태에서 제어를 넘겨받고
;   C 함수 kernel_main()을 호출한다. -> 어셈블리 환경에서 C 커널 코드로 안전하게 넘겨주는 것이 목적으로 이를 위해 Protected Mode의 데이터 세그먼트 레지스터와
; C 코드가 사용할 스택을 초기화한다.
;
; 빌드 예정:
;   nasm -f elf32 kernel_entry.asm -o kernel_entry.o
; ============================================================

[BITS 32]
;kernel_entry를 파일 밖에서도 볼 수 있게 공개하겠다는 의미다.
global kernel_entry
;이 심볼은 외부에 정의되어 있다"고 NASM에 알리주는 코드이다.
extern kernel_main

CODE_SEG equ 0x08
DATA_SEG equ 0x10



section .text

;기존 함수로 되돌아가지 않는 함수이기 때문에 push, pop이 필요가 없다.
kernel_entry:
    ;하드웨어 인터럽트를 차단
    cli

    ; ========================================================
    ; TODO 1
    ; Protected Mode용 data segment selector를
    ; DS / ES / FS / GS / SS에 설정한다.
    ;
    ; 힌트:
    ;   boot11.asm에서 사용하던 DATA_SEG 값과 같아야 한다.
    ;
    ;   ax에 DATA_SEG를 넣은 뒤
    ;   각 segment register에 복사하면 된다.
    ; ========================================================

    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax


    ; ========================================================
    ; TODO 2
    ; C 코드가 사용할 stack을 설정한다.
    ;
    ; ESP = stack의 top 주소
    ; EBP도 처음에는 ESP와 동일하게 맞춰두는 것이 편하다.
    ;
    ; 기존 프로젝트에서 사용하던 안전한 메모리 영역을
    ; stack 위치로 사용한다.
    ;
    ; 예:
    ;   0x90000 부근
    ;
    ; 단, 아래 값을 그대로 복붙하기보다
    ; 왜 이 위치를 쓰는지 생각해 볼 것.
    ; ========================================================
    
    ;실행 중 작업용 stack을 설정함
    mov esp, 0x20000
    mov ebp, esp
 
    ; ========================================================
    ; TODO 3
    ; C 함수 kernel_main() 호출
    ;
    ; kernel_main은 kernel.c에 정의될 예정이다.
    ;
    ; C 함수 호출은 일반적인 call 명령으로 가능하다.
    ; ========================================================

    ; TODO 3 CODE HERE
    call kernel_main


    ; ========================================================
    ; kernel_main이 정상적인 커널이라면 보통 return하지 않는다.
    ;
    ; 하지만 실수로 return할 경우 CPU가 이상한 주소를
    ; 실행하지 않도록 여기서 멈춘다.
    ; ========================================================

.hang:
    ; TODO 4 CODE HERE
    
    ;cli 미래에 위에 코드에 sti추가시 주석을 해체할 필요가 있다.
    ;cpu정지
    hlt
    ;정지가 풀려도 무한 loop을 돌게 만든다.
    jmp .hang