#include"vga.h"
#include"keyboard.h"
#include"interrupts.h"

void kernel_main(void)
{
    char* str = "KERNEL BY PRINT STRING";
    char* str2 = "KERNEL BY PRINT STRING\n WITH ENTER";
    //todo1 test
    /*char info[size] = { 'K', 'E', 'R', 'N', 'E', 'L'};

    for (int i = 0; i < size; i++) {
        put_char(info[i]);
    }*/

    //todo2 test
    //print_string(str);

    //todo3 test
    //print_string(str2);

    //todo4 test
   /* print_string("THIS SHOULD DISAPPEAR");
    delay(5);
    clear_screen();
    print_string("SCREEN CLEARED");*/


    //todo5 test
   /* for (int i = 0; i < 25; i++) {
        print_string("abc\n");
    }

    print_string("the end");*/

    clear_screen();
    print_string("Kernel started\n");
    init_interrupts();
    print_string("Keyboard IRQ ready\n");
    
    while (1){
        
    }
}
