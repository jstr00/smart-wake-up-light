#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdint.h>

/** Start the optional RGB indicator. Missing hardware is nonfatal and leaves it disabled. */
int status_led_init(void);
/** Request an RGB indicator color; if overloaded, the latest color takes precedence. */
void status_led_set(uint8_t red, uint8_t green, uint8_t blue);

#endif
