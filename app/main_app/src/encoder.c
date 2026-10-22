#include "encoder.h"
#include "led_control.h"
#include "logic/encoder_decoder.h"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(encoder, LOG_LEVEL_INF);

static const struct gpio_dt_spec enc_clk = GPIO_DT_SPEC_GET(DT_ALIAS(enc_clk), gpios);
static const struct gpio_dt_spec enc_dt = GPIO_DT_SPEC_GET(DT_ALIAS(enc_dt), gpios);
static const struct gpio_dt_spec enc_sw = GPIO_DT_SPEC_GET(DT_ALIAS(enc_sw), gpios);

static struct gpio_callback clk_cb_data;
static struct gpio_callback dt_cb_data;
static struct gpio_callback sw_cb_data;
static struct k_spinlock input_lock;
static struct encoder_decoder decoder;
static uint32_t last_button_press_ms;
static bool button_seen;
static atomic_t pending_rotation;
static atomic_t pending_presses;
K_SEM_DEFINE(input_signal, 0, 1);
K_THREAD_STACK_DEFINE(input_stack, 1536);
static struct k_thread input_thread;

/* Forward accumulated encoder turns and button presses to the light controller. */
static void input_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    while (true) {
        k_sem_take(&input_signal, K_FOREVER);
        atomic_val_t presses = atomic_set(&pending_presses, 0);
        for (atomic_val_t i = 0; i < presses; ++i) {
            int error = led_control_toggle();
            if (error != 0) {
                LOG_WRN("Button event rejected: %d", error);
            }
        }
        atomic_val_t rotation = atomic_set(&pending_rotation, 0);
        while (rotation != 0) {
            int8_t step = rotation > 0 ? (int8_t)MIN(rotation, 100) : (int8_t)MAX(rotation, -100);
            int error = led_control_adjust_brightness(step);
            if (error != 0) {
                LOG_WRN("Encoder step rejected: %d", error);
                atomic_add(&pending_rotation, rotation);
                k_sleep(K_MSEC(10));
                k_sem_give(&input_signal);
                break;
            }
            rotation -= step;
        }
    }
}

/* Debounce button presses in the GPIO interrupt callback. */
static void sw_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    uint32_t now = k_uptime_get_32();
    k_spinlock_key_t key = k_spin_lock(&input_lock);
    if (button_seen && now - last_button_press_ms < CONFIG_WAKEUP_BUTTON_DEBOUNCE_MS) {
        k_spin_unlock(&input_lock, key);
        return;
    }
    button_seen = true;
    last_button_press_ms = now;
    atomic_inc(&pending_presses);
    k_spin_unlock(&input_lock, key);
    k_sem_give(&input_signal);
}

/* Decode transitions and accumulate detents from GPIO interrupts. */
static void encoder_changed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    k_spinlock_key_t key = k_spin_lock(&input_lock);
    int clk = gpio_pin_get_dt(&enc_clk);
    int dt = gpio_pin_get_dt(&enc_dt);
    if (clk >= 0 && dt >= 0) {
        int8_t step = encoder_decoder_update(&decoder, (uint8_t)((clk << 1) | dt));
        if (step != 0) {
            atomic_add(&pending_rotation, step);
            k_sem_give(&input_signal);
        }
    }
    k_spin_unlock(&input_lock, key);
}

int encoder_init(void) {
    const struct gpio_dt_spec *pins[] = {&enc_clk, &enc_dt, &enc_sw};
    for (size_t i = 0; i < ARRAY_SIZE(pins); ++i) {
        if (!gpio_is_ready_dt(pins[i])) {
            return -ENODEV;
        }
        int error = gpio_pin_configure_dt(pins[i], GPIO_INPUT);
        if (error != 0) {
            return error;
        }
    }
    int clk = gpio_pin_get_dt(&enc_clk);
    int dt = gpio_pin_get_dt(&enc_dt);
    if (clk < 0 || dt < 0) {
        return clk < 0 ? clk : dt;
    }
    encoder_decoder_init(&decoder, (uint8_t)((clk << 1) | dt));
    gpio_init_callback(&clk_cb_data, encoder_changed, BIT(enc_clk.pin));
    gpio_init_callback(&dt_cb_data, encoder_changed, BIT(enc_dt.pin));
    gpio_init_callback(&sw_cb_data, sw_pressed, BIT(enc_sw.pin));
    struct gpio_callback *callbacks[] = {&clk_cb_data, &dt_cb_data, &sw_cb_data};
    for (size_t i = 0; i < ARRAY_SIZE(pins); ++i) {
        int error = gpio_add_callback(pins[i]->port, callbacks[i]);
        if (error != 0) {
            for (size_t j = 0; j < i; ++j) {
                gpio_remove_callback(pins[j]->port, callbacks[j]);
            }
            return error;
        }
    }
    k_thread_create(&input_thread, input_stack, K_THREAD_STACK_SIZEOF(input_stack),
                    input_thread_entry, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
    if (IS_ENABLED(CONFIG_THREAD_NAME)) {
        k_thread_name_set(&input_thread, "light_input");
    }
    for (size_t i = 0; i < ARRAY_SIZE(pins); ++i) {
        int error = gpio_pin_interrupt_configure_dt(pins[i], i == 2 ? GPIO_INT_EDGE_TO_ACTIVE
                                                                    : GPIO_INT_EDGE_BOTH);
        if (error != 0) {
            for (size_t j = 0; j <= i; ++j) {
                gpio_pin_interrupt_configure_dt(pins[j], GPIO_INT_DISABLE);
            }
            for (size_t j = 0; j < ARRAY_SIZE(pins); ++j) {
                gpio_remove_callback(pins[j]->port, callbacks[j]);
            }
            k_thread_abort(&input_thread);
            return error;
        }
    }
    return 0;
}
