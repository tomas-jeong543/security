#include"vga.h"
#include"keyboard.h"
#include"interrupts.h"

void kernel_main(void)
{
    clear_screen();
    print_string("Kernel started\n");

    init_interrupts();

    print_string("KeyBoard IRQ ready\n");
    
    while (1){
        __asm__ volatile ("hlt");
    }
}
