#include "keyboard.h"
#include "lowlevel.h"
#include "vga.h"

void keyboard_handler(void)
{
    unsigned char scancode = inb(0x60);

    // TODO:
    // 일단 make code인지 여부도 신경쓰지 말고
    // 테스트용으로 'K' 같은 문자 하나 출력
    put_char('K');

    // PIC EOI를 c에서 처리하기로 결정함
    outb(0x20, 0x20);
}