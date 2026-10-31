#include"vga.h"
#include"keyboard.h"
#include"interrupts.h"
#include"shell.h"

void kernel_main(void)
{
    clear_screen();
    print_string("Kernel started\n");

    init_interrupts();

    print_string("KeyBoard IRQ ready\n");
    shell_init();

    while (1){
        keyboard_poll();
    }
}
