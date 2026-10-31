#include"shell.h"
#include "keyboard.h"
#include "lowlevel.h"
#include "vga.h"


static void keyboard_process_scancode(unsigned char scancode);

//큐와 관련된 변수들 선언
#define SCANCODE_QUEUE_SIZE 64
static unsigned char scancode_queue[SCANCODE_QUEUE_SIZE];
//데이터를 쓸 위치
static volatile unsigned int queue_head = 0;
//데이터를 읽을 위치
static volatile unsigned int queue_tail = 0;

static const char scancode_table[128] = {
    [0x01] = 27,     // ESC
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',

    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0C] = '-',
    [0x0D] = '=',
    [0x0E] = '\b',   // Backspace
    [0x0F] = '\t',   // Tab

    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 'i',
    [0x18] = 'o',
    [0x19] = 'p',
    [0x1A] = '[',
    [0x1B] = ']',
    [0x1C] = '\n',   // Enter

    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',
    [0x27] = ';',
    [0x28] = '\'',
    [0x29] = '`',
    [0x2B] = '\\',

    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',

    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',
    [0x33] = ',',
    [0x34] = '.',
    [0x35] = '/',
    [0x37] = '*',    // 숫자 키패드 *
    [0x39] = ' ',    // Space
    [0x4A] = '-',    // 숫자 키패드 -
    [0x4E] = '+',    // 숫자 키패드 +
};

//shift key가 눌려있는지 caps_lock이 눌려있는지를 나타내는 변수
static int left_shift = 0;
static int right_shift = 0;
static int caps_lock = 0;
static int extended = 0;

static void queue_push(unsigned char scancode)
{
    unsigned int next_head;

    // 다음 tail 위치 계산
    next_head = (queue_head + 1) % SCANCODE_QUEUE_SIZE;
    
    // 큐가 가득 찼는지 검사하고 가득 찼다면 새로운 입력을 버리고 return
    if (next_head == queue_tail)
        return;
    
    // 현재 head 위치에 scancode 저장
    scancode_queue[queue_head] = scancode;
    
    // head를 다음 위치로 갱신
    queue_head = next_head;
}

static int queue_pop(unsigned char* scancode)
{
    unsigned int next_tail;

    next_tail = (queue_tail + 1) % SCANCODE_QUEUE_SIZE;
    // 큐가 비어 있으면 0 반환
    if (queue_tail == queue_head) {
        return 0;
    }
    // tail 위치의 스캔코드를 *scancode에 저장
    *scancode = scancode_queue[queue_tail];
    // tail을 다음 위치로 이동
    queue_tail = next_tail;
    // 성공했다면 1 반환
    return 1;
}

static void check_shiftlock(unsigned char scancode)
{
    if (scancode == 0x2A ) {
        left_shift = 1;
    }
    else if (scancode == 0xAA) {
        left_shift = 0;
    }
    
    if (scancode == 0x36) {
        right_shift =  1;
    }
    else if (scancode == 0xB6) {
        right_shift = 0;
    }

    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
    }

}

void keyboard_poll(void)
{
    unsigned char scancode;

    // queue_pop()이 성공하는 동안 반복
    while (queue_pop(&scancode)) {
        keyboard_process_scancode(scancode);
    }
}

// 기존 keyboard_handler()에서 check_shiftlock(scancode)부터 ASCII 변환, 입력 버퍼, Shell 실행하는 기능을 여기로 옮김
static void keyboard_process_scancode(unsigned char scancode)
{
    check_shiftlock(scancode);

    int shift_pressed = 0;
    if (left_shift || right_shift) {
        shift_pressed = 1;
    }

    int uppercase = shift_pressed ^ caps_lock;

    if (scancode == 0xE0) {
        extended = 1;
    }
    else if (extended) {
        extended = 0;

        if (scancode == 0x4B) {
            //왼쪽 커서 이동 함수 호출
            move_cursor_left();
        }
        else if (scancode == 0x4D) {
            //오른쪽 커서 이동 함수 호출
            move_cursor_right();
        }
    }
    else {
        //좀 더 자세히 설명시 make code = 0x1E, break code = 0x9E라서 0x80 = 1000 0000 과 &연산을 하면
        // make = 0000 0000이 break = 1000 0000이 되서 이 성질을 활용해서 이러한 조건문을 작성한 거다.
        if ((scancode & 0x80) == 0) {
            // Scancode를 ASCII로 변환
            char ascii = scancode_table[scancode];

            if (ascii >= 'a' && ascii <= 'z' && uppercase) {
                ascii -= 32;
            }
            //shell에서 문자열 처리
            shell_handle_char(ascii);
        }
    }
}


void keyboard_handler(void)
{    
    unsigned char scancode = inb(0x60);
    
    //IRQ입력에 대한 처리
    queue_push(scancode);

    outb(0x20, 0x20);
}