#include "keyboard.h"
#include "lowlevel.h"
#include "vga.h"

void keyboard_handler(void)
{
    unsigned char scancode = inb(0x60);

    // TODO 17-4 scancode의 최상위 비트가 0이라면  put_char('K') 실행
    //좀 더 자세히 설명시 make code = 0x1E, break code = 0x9E라서 0x80 = 1000 0000 과 &연산을 하면
    // make = 0000 0000이 break = 1000 0000이 되서 이 성질을 활용해서 이러한 조건문을 작성한 거다.
    if ((scancode & 0x80) == 0) {
        put_char('K');
    }
    outb(0x20, 0x20);
}