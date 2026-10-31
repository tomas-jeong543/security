#ifndef SHELL_H
#define SHELL_H

void shell_execute(const char* command);
void shell_handle_char(char ascii);
void shell_init(void);
#endif