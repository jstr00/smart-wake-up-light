#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>

/** Initialize the PWM channels and start the command workqueue. Return 0 on success or a negative
 * errno. */
int led_control_init(void);
/** Queue a toggle between OFF and NIGHTLIGHT; during WAKEUP, switch OFF. Return 0 if queued,
 * negative errno otherwise. */
int led_control_toggle(void);
/** Queue an encoder brightness delta; positive values brighten warm white. Return 0 if queued,
 * negative errno otherwise. */
int led_control_adjust_brightness(int8_t step);
/** Queue a new wake-up animation of duration_s seconds; duration must be within configured limits.
 * Return 0 if queued, negative errno otherwise. */
int led_control_start_wakeup(uint32_t duration_s);

#endif
