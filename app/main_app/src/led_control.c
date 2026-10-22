#include "led_control.h"
#include "logic/light_model.h"

#include <errno.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_control, LOG_LEVEL_INF);

static const struct pwm_dt_spec pwm_ww = PWM_DT_SPEC_GET(DT_ALIAS(pwm_ww));
static const struct pwm_dt_spec pwm_cw = PWM_DT_SPEC_GET(DT_ALIAS(pwm_cw));
static struct light_state light;
static uint16_t wakeup_progress;
static int64_t wakeup_started_ms;
static uint64_t wakeup_duration_ms;
static struct k_work_delayable wakeup_work;
static struct k_work command_work;
static struct k_work_q light_queue;
static bool initialized;
K_THREAD_STACK_DEFINE(light_stack, 2048);

enum command_type { COMMAND_TOGGLE, COMMAND_ADJUST, COMMAND_WAKEUP };

struct light_command {
    enum command_type type;
    int8_t step;
    uint32_t duration_s;
};

K_MSGQ_DEFINE(command_msgq, sizeof(struct light_command), 32, 4);

/* Calculate and apply PWM outputs for the current state. */
static void update_leds(void) {
    struct light_levels levels = light_model_output(&light, wakeup_progress);
    uint32_t warm_pulse = (uint32_t)(((uint64_t)pwm_ww.period * levels.warm_duty_bp) / 10000);
    uint32_t cold_pulse = (uint32_t)(((uint64_t)pwm_cw.period * levels.cold_duty_bp) / 10000);
    int warm_err = pwm_set_pulse_dt(&pwm_ww, warm_pulse);
    int cold_err = pwm_set_pulse_dt(&pwm_cw, cold_pulse);
    if (warm_err || cold_err) {
        LOG_ERR("PWM update failed: warm=%d cold=%d", warm_err, cold_err);
    }
}

/* Advance the time-based wake-up animation on the light workqueue. */
static void wakeup_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (light.mode != LIGHT_MODE_WAKEUP) {
        return;
    }
    uint64_t elapsed_ms = (uint64_t)(k_uptime_get() - wakeup_started_ms);
    if (elapsed_ms >= wakeup_duration_ms) {
        wakeup_progress = 1000;
        update_leds();
        LOG_INF("Wake-up complete");
        return;
    }
    wakeup_progress = (uint16_t)((elapsed_ms * 1000) / wakeup_duration_ms);
    update_leds();
    uint64_t next_step_ms = MAX(1ULL, (wakeup_duration_ms / CONFIG_WAKEUP_ANIMATION_STEPS));
    uint64_t remaining_ms = wakeup_duration_ms - elapsed_ms;
    uint32_t delay_ms = (uint32_t)MIN(next_step_ms, remaining_ms);
    k_work_reschedule_for_queue(&light_queue, &wakeup_work, K_MSEC(delay_ms));
}

/* Process queued commands in order and update light state. */
static void command_handler(struct k_work *work) {
    ARG_UNUSED(work);
    struct light_command command;
    unsigned int budget = 16;
    while (budget-- && k_msgq_get(&command_msgq, &command, K_NO_WAIT) == 0) {
        k_work_cancel_delayable(&wakeup_work);
        switch (command.type) {
        case COMMAND_TOGGLE:
            light_model_toggle(&light);
            break;
        case COMMAND_ADJUST:
            light_model_adjust(&light, command.step);
            break;
        case COMMAND_WAKEUP:
            light_model_start_wakeup(&light);
            wakeup_progress = 0;
            wakeup_started_ms = k_uptime_get();
            wakeup_duration_ms = (uint64_t)command.duration_s * 1000ULL;
            k_work_reschedule_for_queue(&light_queue, &wakeup_work, K_MSEC(1));
            break;
        }
        update_leds();
    }
    if (k_msgq_num_used_get(&command_msgq) != 0) {
        k_work_submit_to_queue(&light_queue, &command_work);
    }
}

/* Enqueue a light command or report backpressure. */
static int submit_command(struct light_command command) {
    if (!initialized) {
        return -EAGAIN;
    }
    int error = k_msgq_put(&command_msgq, &command, K_NO_WAIT);
    if (error != 0) {
        LOG_WRN("Light command queue full: %d", error);
        return -ENOBUFS;
    }
    int submitted = k_work_submit_to_queue(&light_queue, &command_work);
    if (submitted < 0) {
        LOG_ERR("Light command submission failed: %d", submitted);
        return submitted;
    }
    return 0;
}

int led_control_init(void) {
    if (!pwm_is_ready_dt(&pwm_ww) || !pwm_is_ready_dt(&pwm_cw)) {
        return -ENODEV;
    }
    light_model_init(&light, CONFIG_WAKEUP_DEFAULT_BRIGHTNESS);
    wakeup_progress = 0;
    int error_warm = pwm_set_pulse_dt(&pwm_ww, 0);
    int error_cold = pwm_set_pulse_dt(&pwm_cw, 0);
    if (error_warm || error_cold) {
        LOG_ERR("Initial PWM off failed: warm=%d cold=%d", error_warm, error_cold);
        return -EIO;
    }
    k_work_init_delayable(&wakeup_work, wakeup_handler);
    k_work_init(&command_work, command_handler);
    k_work_queue_start(&light_queue, light_stack, K_THREAD_STACK_SIZEOF(light_stack), 5, NULL);
    if (IS_ENABLED(CONFIG_THREAD_NAME)) {
        k_thread_name_set(&light_queue.thread, "light_control");
    }
    initialized = true;
    return 0;
}

int led_control_toggle(void) {
    return submit_command((struct light_command){.type = COMMAND_TOGGLE});
}

int led_control_adjust_brightness(int8_t step) {
    return submit_command((struct light_command){.type = COMMAND_ADJUST, .step = step});
}

int led_control_start_wakeup(uint32_t duration_s) {
    if (duration_s == 0 || duration_s > CONFIG_WAKEUP_MAX_DURATION_S) {
        return -EINVAL;
    }
    return submit_command((struct light_command){.type = COMMAND_WAKEUP, .duration_s = duration_s});
}
