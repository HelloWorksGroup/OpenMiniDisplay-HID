#ifndef _DISPLAY_DRV_H_
#define _DISPLAY_DRV_H_

#include <stdint.h>

void display_init(void);
void display_set_y(int8_t y);
void display_number(uint32_t number);
void display_str(const char* str, uint8_t length);
void display_raw(uint8_t* raw);

#endif
