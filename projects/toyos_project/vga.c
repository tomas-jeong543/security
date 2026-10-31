#include"vga.h"
#include "lowlevel.h"
#define MAX_ROW 25
#define MAX_COL 80
#define size 6

//clear screen에 맞춰서 row와 col을 초기화하기 위해서 프로그램 전체의 전역변수로 설정함
static int row = 0;
static int col = 0;


static void update_cursor(void)
{
    // TODO 19-1: position 계산
    int position = row * 80 + col;
    // TODO 19-2: outb()로 VGA 커서 위치 갱신
    
    
    // 하위 8비트 설정 -> 현재 vga에서의 위치 정보를 이용
    outb(0x3D4, 0x0F);
    outb(0x3D5, position & 0xFF);

    // 상위 8비트 설정 -> 위와 동일
    outb(0x3D4, 0x0E);
    outb(0x3D5, (position >> 8) & 0xFF);
}

void put_char(char c)
{
    volatile unsigned short* vga =
        (volatile unsigned short*)0xB8000;

    int index;

    // Backspace
    if (c == '\b') {
        if (col > 0) {
            col--;
            index = row * MAX_COL + col;
            vga[index] = (0x07 << 8) | ' ';
        }
        update_cursor();
        return;
    }

    // Enter
    if (c == '\n') {
        if (row < MAX_ROW - 1) {
            row++;
            col = 0;
        }
        else {
            scroll_screen();
        }

        update_cursor();
        return;
    }

    // 일반 문자
    index = row * MAX_COL + col;
    vga[index] = (0x07 << 8) | (unsigned char)c;

    // 다음 출력 위치로 이동
    if (col < MAX_COL - 1) {
        col++;
    }
    else if (row < MAX_ROW - 1) {
        col = 0;
        row++;
    }
    else {
        scroll_screen();
    }

    update_cursor();
    return;
}

void scroll_screen(void) {
    volatile unsigned short* vga =
        (volatile unsigned short*)0xB8000;

    for (int i = 0; i < 25; i++) {
        if (i != 24) {
            for (int j = 0; j < 80; j++) {
                vga[i * 80 + j] = vga[(i + 1) * 80 + j];
            }
        }
        else {
            for (int j = 0; j < 80; j++) {
                vga[i * 80 + j] = (0x07 << 8) | ' ';
            }
        }
    }

    row = 24;
    col = 0;
    update_cursor();
}

void print_string(const char* str)
{
    int i = 0;

    while (str[i] != '\0') {
        put_char(str[i++]);
    }
}

void clear_screen(void)
{
    volatile unsigned short* vga =
        (volatile unsigned short*)0xB8000;

    for (int i = 0; i < MAX_ROW * MAX_COL; i++) {
        vga[i] = (0x07 << 8) | ' ';
    }

    row = 0;
    col = 0;
    update_cursor();
}

void move_cursor_left(void)
{
    if (col > 0) {
        col -= 1;
    }
    update_cursor();
    return;
}


void move_cursor_right(void)
{
    if (col < MAX_COL - 1) {
        col += 1;
    }
    update_cursor();
    return;
}
