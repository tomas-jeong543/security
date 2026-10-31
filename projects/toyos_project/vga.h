#ifndef VGA_H
#define VGA_H


void put_char(char c);
void print_string(const char* str);
void clear_screen(void);
void scroll_screen(void);
void move_cursor_left(void);
void move_cursor_right(void);

#endif