#include"shell.h"
#include"vga.h"

#define COMMAND_COUNT \
    (sizeof(command_table) / sizeof(command_table[0]))
#define INPUT_SIZE 128


//명령어 구조체 -> 이름, 핸들러 그리고 help명령어에서 보여줄 설명을 가지고 있다.;
typedef struct {
    const char* name;
    void (*handler)(const char* args);
    const char* description;
} Command;

//명령어 처리 관련 핸들러 함수
void cmd_help(const char* args);
void cmd_clear(const char* args);
void cmd_version(const char* args);
void cmd_echo(const char* args);


static int strings_equal(const char* a, const char* b);
static int command_equal(const char* a, const char* b);
void shell_handle_char(char ascii);
static void shell_print_prompt(void);
void shell_init(void);

//버퍼 관련된 전역 변수
static char input_buffer[INPUT_SIZE];
static int input_index = 0;

static const Command command_table[] = {
    {"help", cmd_help, "Show available commands"},
    {"clear", cmd_clear, "Clear the screen"},
    {"version",cmd_version, "Print the version of OS"},
    {"echo", cmd_echo, "Print text"}
};



//문자열 비교 함수
static int strings_equal(const char* a, const char* b)
{
    int idx = 0;
    
    while (!(a[idx] == b[idx] && a[idx] == '\0')) {
        if (a[idx] != b[idx]) {
            return 0;
        }
        else {
            idx++;
        }
    }
    return 1;
}

static int command_equal(const char* input, const char* command)
{
    int idx = 0;

    while (!(input[idx] == ' ' || input[idx] == '\0')) {
        if (input[idx] != command[idx]) {
            return 0;
        }
        else {
            idx++;
        }
    }
    return command[idx] == '\0' ? idx : 0;
}


void shell_execute(const char* command)
{
    int idx;
   
    for (int i = 0; i < COMMAND_COUNT; i++) {
      
        idx = command_equal(command, command_table[i].name);

        if (idx > 0) {

            const char* ptr = command + idx;

            // 인수 앞의 공백 제거
            while (*ptr == ' ') {
                ptr++;
            }
            
            command_table[i].handler(ptr);


            return;
        }
    }

    print_string("Unknown command\n");
}

void cmd_help(const char* args) {
    
    (void)args;  // 사용하지 않는 인수

    for (int i = 0; i < COMMAND_COUNT; i++) {
        print_string(command_table[i].name);
        print_string(": ");
        print_string(command_table[i].description);
        put_char('\n');
    }
}
void cmd_clear(const char* args) {

    (void)args;  // 사용하지 않는 인수
    clear_screen();
}

void cmd_version(const char* args)
{
    (void)args;  // 사용하지 않는 인수
    print_string("ToyOS Version 0.1\n");
}

void cmd_echo(const char* args)
{
    print_string(args);
    put_char('\n');


}

void shell_handle_char(char ascii)
{
    // Backspace 입력의 경우
    if (ascii == '\b') {
        if (input_index > 0) {
            input_index--;
            input_buffer[input_index] = '\0';
            put_char('\b');
        }
        return;
    }
    
    // Enter 입력의 경우
    if (ascii == '\n') {
        input_buffer[input_index] = '\0';
        put_char('\n');
        if (input_index > 0) {
          
            shell_execute(input_buffer);
        }
        input_index = 0;
        shell_print_prompt();
        return;
    }

    // 출력 가능한 일반 문자일 경우
    if (ascii >= 32 && ascii <= 126) {
        if (input_index < INPUT_SIZE - 1) {
            input_buffer[input_index++] = ascii;
            put_char(ascii);
        }
    }


}

static void shell_print_prompt(void)
{
    print_string("Toy_OS> ");
}

void shell_init(void)
{
    shell_print_prompt();
}

