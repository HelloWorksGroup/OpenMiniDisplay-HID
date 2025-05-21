
#include "display_drv.h"
#include "pico/stdlib.h"

#define CS_PIN 28
#define CLK_PIN 27
#define DATA_PIN 29
// SPI CPOL=LOW CPHA=LOW

const uint8_t setup_normal_mode[] = { 0xC, 0x1, 0xC, 0x1, 0xC, 0x1, 0xC, 0x1 }; // normal mode
const uint8_t setup_scan_limit[] = { 0xB, 0x7, 0xB, 0x7, 0xB, 0x7, 0xB, 0x7 };  // scan limit
uint8_t setup_duty[] = { 0xA, 0x2, 0xA, 0x2, 0xA, 0x2, 0xA, 0x2 };              // duty 5/32
const uint8_t setup_no_decodes[] = { 0x9, 0x0, 0x9, 0x0, 0x9, 0x0, 0x9, 0x0 };  // no decodes
const uint8_t setup_test_off[] = { 0xF, 0x0, 0xF, 0x0, 0xF, 0x0, 0xF, 0x0 };    // test off

uint8_t display_cache[32];
uint8_t display_buffer[32];

int8_t display_y = 0;

static void spi_init(void) {
	gpio_init(CS_PIN);
	gpio_set_dir(CS_PIN, GPIO_OUT);
	gpio_put(CS_PIN, 1);

	gpio_init(CLK_PIN);
	gpio_set_dir(CLK_PIN, GPIO_OUT);
	gpio_put(CLK_PIN, 0);

	gpio_init(DATA_PIN);
	gpio_set_dir(DATA_PIN, GPIO_OUT);
	gpio_put(DATA_PIN, 0);
}

static void spi_byte(uint8_t data) {
	for(int i = 0; i < 8; i++) {
		gpio_put(DATA_PIN, data & (0x80 >> i));
		gpio_put(CLK_PIN, 1);
		sleep_us(1);
		gpio_put(CLK_PIN, 0);
		sleep_us(1);
	}
}

static void spi_send(uint8_t* array, uint16_t length) {
	gpio_put(CS_PIN, 0);
	sleep_us(1);
	for(int i = 0; i < length; i++) {
		spi_byte(array[i]);
	}
	gpio_put(CS_PIN, 1);
	sleep_us(1);
}
static void display_set_duty(uint8_t duty) {
	for(int i = 0; i < 4; i++) {
		setup_duty[i * 2 + 1] = duty;
	}
	spi_send(setup_duty, count_of(setup_duty));
}

static const uint8_t number_font[10][3] = {
	{0xF0, 0x88, 0x78}, // 0
	{0x40, 0xF8, 0x00}, // 1
	{0x98, 0xA8, 0xC8}, // 2
	{0xA8, 0xA8, 0xD0}, // 3
	{0xE0, 0x20, 0xF8}, // 4
	{0xC8, 0xA8, 0x90}, // 5
	{0x78, 0xA8, 0xB0}, // 6
	{0x80, 0xB8, 0xC0}, // 7
	{0xF0, 0xA8, 0x78}, // 8
	{0xE8, 0xA8, 0x78}, // 9
};

static const uint8_t asc_simple[][3] = {
	{0x00, 0x00, 0x00}, // " "
	{0x00, 0xE8, 0x00}, // "!"
	{0xC0, 0x00, 0xC0}, // """
	{0x50, 0x00, 0x50}, // "#"
	{0xE8, 0xF8, 0xB8}, // "$"
	{0x18, 0xD8, 0xC0}, // "%"
	{0x68, 0x90, 0x48}, // "&"
	{0x00, 0xC0, 0x00}, // "'"
	{0x00, 0x70, 0x88}, // "("
	{0x88, 0x70, 0x00}, // ")"
	{0x28, 0x70, 0x28}, // "*"
	{0x20, 0x70, 0x20}, // "+"
	{0x08, 0x10, 0x00}, // ","
	{0x20, 0x20, 0x20}, // "-"
	{0x00, 0x10, 0x00}, // "."
	{0x10, 0x20, 0x40}, // "/"
	{0xF0, 0x88, 0x78}, // "0"
	{0x40, 0xF8, 0x00}, // "1"
	{0x98, 0xA8, 0xC8}, // "2"
	{0xA8, 0xA8, 0xD0}, // "3"
	{0xE0, 0x20, 0xF8}, // "4"
	{0xC8, 0xA8, 0x90}, // "5"
	{0x78, 0xA8, 0xB0}, // "6"
	{0x80, 0xB8, 0xC0}, // "7"
	{0xF0, 0xA8, 0x78}, // "8"
	{0xE8, 0xA8, 0x78}, // "9"
	{0x00, 0x50, 0x00}, // ":"
	{0x00, 0x58, 0x00}, // ":"
	{0x20, 0x50, 0x88}, // "<"
	{0x50, 0x50, 0x50}, // "="
	{0x88, 0x50, 0x20}, // ">"
	{0x80, 0xA8, 0xC0}, // "?"
	{0xF8, 0xE8, 0xE8}, // "@"
	{0x78, 0xA0, 0x78}, // "A"
	{0xF8, 0xA8, 0x50}, // "B"
	{0x70, 0x88, 0x88}, // "C"
	{0xF8, 0x88, 0x70}, // "D"
	{0xF8, 0xA8, 0x88}, // "E"
	{0xF8, 0xA0, 0x80}, // "F"
	{0x70, 0x88, 0xB8}, // "G"
	{0xF8, 0x20, 0xF8}, // "H"
	{0x88, 0xF8, 0x88}, // "I"
	{0x10, 0x08, 0xF0}, // "J"
	{0xF8, 0x20, 0xD8}, // "K"
	{0xF8, 0x08, 0x08}, // "L"
	{0xF8, 0x40, 0xF8}, // "M"
	{0xF8, 0x80, 0x78}, // "N"
	{0x70, 0x88, 0x70}, // "O"
	{0xF8, 0xA0, 0x40}, // "P"
	{0x70, 0x88, 0x78}, // "Q"
	{0xF8, 0xA0, 0x58}, // "R"
	{0x48, 0xA8, 0x90}, // "S"
	{0x80, 0xF8, 0x80}, // "T"
	{0xF8, 0x08, 0xF8}, // "U"
	{0xF0, 0x08, 0xF0}, // "V"
	{0xF8, 0x10, 0xF8}, // "W"
	{0xD8, 0x20, 0xD8}, // "X"
	{0xC0, 0x38, 0xC0}, // "Y"
	{0x98, 0xA8, 0xC8}  // "Z"
};

static void display_clear(void) {
	for(int i = 0; i < 32; i++) {
		display_buffer[i] = 0;
		display_cache[i] = 0;
	}
}

static void display_cache_pop(void) {
	for(int i = 0; i < 32; i++) {
		if(display_y >= 0) {
			display_y = display_y > 8 ? 8 : display_y;
			display_buffer[i] = display_cache[i] >> display_y;
		} else {
			display_y = display_y < -8 ? -8 : display_y;
			display_buffer[i] = display_cache[i] << -display_y;
		}
	}
}
static void display_update(void) {
	uint8_t buffer[8];
	for(int x = 0; x < 8; x++) {
		for(int i = 0; i < 4; i++) {
			buffer[i * 2] = x + 1;
			buffer[i * 2 + 1] = display_buffer[31 - i * 8 - x];
		}
		spi_send(buffer, count_of(buffer));
	}
}

void display_str(const char* str, uint8_t length) {
	if(length > 8) {
		length = 8;
	}
	uint8_t offset = (32 - (length * 4 - 1)) / 2;
	display_clear();
	for(int i = 0; i < length; i++) {
		if(str[i] >= ' ' && str[i] <= 'Z') {
			for(int j = 0; j < 3; j++) {
				display_cache[offset + i * 4 + j] = asc_simple[str[i] - ' '][j];
			}
		}
	}
	display_cache_pop();
	display_update();
}
void display_number(uint32_t number) {
	uint8_t numbers[8];
	uint8_t digits = 0;
	while(number > 0) {
		numbers[digits++] = number % 10;
		number /= 10;
		if(digits >= 8) break;
	}
	uint8_t offset = (32 - (digits * 4 - 1)) / 2;
	display_clear();
	for(int i = 0; i < digits; i++) {
		for(int j = 0; j < 3; j++) {
			display_cache[offset + i * 4 + j] = number_font[numbers[digits - 1 - i]][j];
		}
	}
	display_cache_pop();
	display_update();
}
void display_raw(uint8_t* raw) {
	for(int i = 0; i < 32; i++) {
		display_buffer[i] = raw[i];
	}
	display_update();
}
void display_set_y(int8_t y) {
	display_y = y;
}

void display_init(void) {
	spi_init();
	spi_send(setup_normal_mode, count_of(setup_normal_mode));
	spi_send(setup_scan_limit, count_of(setup_scan_limit));
	display_set_duty(2);
	spi_send(setup_no_decodes, count_of(setup_no_decodes));
	spi_send(setup_test_off, count_of(setup_test_off));
	display_set_y(2);
}
