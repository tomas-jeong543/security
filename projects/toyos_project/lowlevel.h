#ifndef LOWLEVEL_H
#define LOWLEVEL_H
//하드웨어에서 i/o입력을 1바이트 받는 함수    
unsigned char inb(unsigned short port);
//하드웨어에서 i/o출력을 1바이트 내보내는 함수    
void outb(unsigned short port, unsigned char value);

#endif
