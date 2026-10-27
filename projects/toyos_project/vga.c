#include"vga.h"
#define MAX_ROW 25
#define MAX_COL 80
#define size 6

//clear screen에 맞춰서 row와 col을 초기화하기 위해서 프로그램 전체의 전역변수로 설정함
static int row = 0;
static int col = 0;

void put_char(char c) {

    // 물리주소 0xB8000부터 시작하는 VGA 텍스트 메모리를, 2바이트짜리 문자 셀 배열처럼(unsigned short) 포인터로 직접 접근하겠다.
    // volatile은 이 접근은 하드웨어와 연결돼 있으니 컴파일러가 최적화로 없애지 마라는 의미이다.
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;

    // 인덱스 즉 출력 위치를 행과 열의 숫자를 기반으로 설정하게 만들었다.
    int index = 0;

    //개행 문자가 들어왔을 때의 처리
    if (c == '\n') {

        if (row < MAX_ROW - 1) {
            row++;
            col = 0;
        }
        else
            scroll_screen();
        return;
    }

    // 현재 커서 위치 계산  index = row * 80 + col
    index = row * 80 + col;
    // 문자 + 색상(attribute)을 16비트 하나로 합쳐서 저장 vga[index] = ...
    vga[index] = (0x07 << 8) | c;

    // 다음 칸으로 커서 이동 -> 가득차면 다음줄로 이동하게 설정을 함
    if (col < 79)
        col++;
    else if (row == MAX_ROW - 1)
        scroll_screen();
    else {
        col = 0;
        row++;
    }
    return;
}

void print_string(const char* str)
{
    int i = 0;
    /* TODO: 문자열의 끝이 아닐 동안 */
    while (str[i] != '\0') {
        // TODO: 현재 문자 str[i]를 put_char()로 출력
        put_char(str[i++]);
    }
}

void clear_screen(void)
{
    volatile unsigned short* vga =
        (volatile unsigned short*)0xB8000;

    for (int i = 0; i < MAX_ROW * MAX_COL; i++) {
        // TODO:
        // 각 셀을 공백 + 0x07 attribute로 덮기
        vga[i] = (0x07 << 8) | ' ';
    }

    // TODO:
    // 출력 위치도 다시 맨 왼쪽 위로 돌려야 함
    row = 0;
    col = 0;
    return;
}

//일종의 sleep 기능을 하는 함수다.
void delay(int n)
{
    for (volatile int i = 0; i < 100000000U * n; i++) {
    }
}

//스크롤 이동하는 함수
void scroll_screen(void) {
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    for (int i = 0; i < 25; i++) {
        //한 칸씩 위로 밈
        if (i != 24) {
            for (int j = 0; j < 80; j++) {
                vga[i * 80 + j] = vga[(i + 1) * 80 + j];
            }
        }//맨 아랫줄을 공백으로 설정함
        else {
            for (int j = 0; j < 80; j++) {
                vga[i * 80 + j] = (0x07 << 8) | ' ';
            }
        }
    }
    row = 24;
    col = 0;
    return;
}
