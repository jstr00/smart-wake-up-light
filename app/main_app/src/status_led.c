#include "status_led.h"

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(status_led, LOG_LEVEL_INF);

#if DT_HAS_ALIAS(led_strip)
static const struct device *const strip = DEVICE_DT_GET(DT_ALIAS(led_strip));
#else
static const struct device *const strip = NULL;
#endif

struct led_color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

K_MSGQ_DEFINE(status_queue, sizeof(struct led_color), 4, 4);
K_THREAD_STACK_DEFINE(status_stack, 1024);
static struct k_thread status_thread;
static bool available;

/* Apply the latest requested indicator color without blocking the caller. */
static void status_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    struct led_color msg;
    int previous_error = 0;
    while (true) {
        k_msgq_get(&status_queue, &msg, K_FOREVER);
        while (k_msgq_get(&status_queue, &msg, K_NO_WAIT) == 0) {
        }
        struct led_rgb color = {.r = msg.red, .g = msg.green, .b = msg.blue};
        int error = led_strip_update_rgb(strip, &color, 1);
        if (error != previous_error) {
            if (error != 0) {
                LOG_WRN("Status LED write failed: %d", error);
            } else if (previous_error != 0) {
                LOG_INF("Status LED recovered");
            }
            previous_error = error;
        }
    }
}

int status_led_init(void) {
    if (strip == NULL || !device_is_ready(strip)) {
        LOG_WRN("Status LED unavailable; running without indicator");
        return 0;
    }
    available = true;
    k_thread_create(&status_thread, status_stack, K_THREAD_STACK_SIZEOF(status_stack),
                    status_thread_entry, NULL, NULL, NULL, 7, 0, K_NO_WAIT);
    if (IS_ENABLED(CONFIG_THREAD_NAME)) {
        k_thread_name_set(&status_thread, "status_led");
    }
    return 0;
}

void status_led_set(uint8_t red, uint8_t green, uint8_t blue) {
    if (!available) {
        return;
    }
    struct led_color msg = {.red = red, .green = green, .blue = blue};
    if (k_msgq_put(&status_queue, &msg, K_NO_WAIT) != 0) {
        struct led_color discarded;
        k_msgq_get(&status_queue, &discarded, K_NO_WAIT);
        if (k_msgq_put(&status_queue, &msg, K_NO_WAIT) != 0) {
            LOG_WRN("Status LED event dropped");
        }
    }
}
