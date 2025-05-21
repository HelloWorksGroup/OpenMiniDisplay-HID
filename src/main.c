
#include "pico/stdlib.h"
#include "hardware/sync.h"

#include "scheduler/uevent.h"
#include "scheduler/scheduler.h"

#include "platform.h"
#include "led_drv.h"

#include "tusb_config.h"

#include "pico/sync.h"
#include "pico/float.h"
#include "pico/bootrom.h"

#include "display_drv.h"

critical_section_t scheduler_lock;
static __inline void CRITICAL_REGION_INIT(void) {
	critical_section_init(&scheduler_lock);
}
static __inline void CRITICAL_REGION_ENTER(void) {
	critical_section_enter_blocking(&scheduler_lock);
}
static __inline void CRITICAL_REGION_EXIT(void) {
	critical_section_exit(&scheduler_lock);
}

bool timer_4hz_callback(struct repeating_timer* t) {
	static uint8_t slower = 0;
	if((++slower & 0x7) == 0) {
		LOG_RAW("At %lld us:\n", time_us_64());
	}
	uevt_bc_e(UEVT_TIMER_4HZ);
	return true;
}
bool timer_fps_callback(struct repeating_timer* t) {
	uevt_bc_e(UEVT_TIMER_FPS);
	return true;
}

#define U32RGB(r, g, b) (((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b))

void led_blink_routine(void) {
	static uint8_t _tick = 0;
	_tick += 1;
	if(_tick & 0x1) {
		if(usb_mounted) {
			ws2812_setpixel(U32RGB(4, 14, 4));
		} else {
			ws2812_setpixel(U32RGB(20, 20, 2));
		}
	} else {
		ws2812_setpixel(U32RGB(0, 0, 0));
	}
}

void main_handler(uevt_t* evt) {
	static uint32_t number = 0;
	switch(evt->evt_id) {
		case UEVT_TIMER_4HZ:
			led_blink_routine();
			break;
		case UEVT_TIMER_FPS:
			// display_number(number++);
			break;
	}
}

void uevt_log(char* str) {
	LOG_RAW("%s\n", str);
}

void hid_parser(uint8_t const* buffer, uint16_t bufsize) {
	if(buffer[0] != 0x5E) {
		return;
	}
	uint8_t len = buffer[1];
	// type 0xCD = command
	if(buffer[2] == 0xCD) {
		switch(buffer[3]) {
			case 'n': // number
				LOG_RAW("Display Number\n");
				display_number(buffer[4] | (buffer[5] << 8) | (buffer[6] << 16) | (buffer[7] << 24));
				break;
			case 's': // string
				LOG_RAW("Display String\n");
				display_str((char*)&buffer[4], len - 2);
				break;
			case 'r': // raw
				LOG_RAW("Display Raw\n");
				display_raw(&buffer[4]);
				break;
			case 'y': // y
				LOG_RAW("Display SET Y\n");
				display_set_y(buffer[4]);
				break;
		}
	}
}

const char printHex[] = "0123456789ABCDEF";
void hid_receive(uint8_t const* buffer, uint16_t bufsize) {
	char str[16 * 2 + 1];
	str[32] = 0;
	for(uint16_t i = 0; i < 16; i++) {
		str[i * 2] = printHex[buffer[i] >> 4];
		str[i * 2 + 1] = printHex[buffer[i] & 0xF];
	}
	// print first 32 bytes
	LOG_RAW("HID[%d]:%s\n", bufsize, str);
	hid_parser(buffer, bufsize);

	uint8_t echo[64];
	// echo back with every byte + 1
	for(uint16_t i = 0; i < bufsize; i++) {
		echo[i] = buffer[i] + 1;
	}
	hid_send(echo, bufsize);
}

static char serial_fifo[16];
static uint8_t serial_wp = 0;
uint8_t serial_got(const char* str) {
	uint8_t len = strlen(str);
	for(uint8_t i = 1; i <= len; i++) {
		if(serial_fifo[serial_wp + (0x10 - i) & 0xF] != str[len - i]) {
			return 0;
		}
	}
	return 1;
}
void serial_receive(uint8_t const* buffer, uint16_t bufsize) {
	for(uint16_t i = 0; i < bufsize; i++) {
		if((*buffer == 0x0A) || (*buffer == 0x0D)) {
			if(serial_got("UPLOAD")) {
				ws2812_setpixel(U32RGB(20, 0, 20));
				reset_usb_boot(0, 0);
			}
		} else {
			serial_fifo[serial_wp++ & 0xF] = *buffer++;
		}
	}
}

#include "hardware/xosc.h"
extern void cdc_task(void);
const char ready[] = "READY";
int main() {
	xosc_init();

	CRITICAL_REGION_INIT();
	app_sched_init();
	user_event_init();
	user_event_handler_regist(main_handler);

	ws2812_setup();
	display_init();
	display_str(ready, 5);

	struct repeating_timer timer;
	add_repeating_timer_us(249978ul, timer_4hz_callback, NULL, &timer);
	struct repeating_timer timerFPS;
	add_repeating_timer_us(16978ul, timer_fps_callback, NULL, &timerFPS);
	tusb_init();
	cdc_log_init();
	while(true) {
		app_sched_execute();
		tud_task();
		cdc_task();
		__wfi();
	}
}
