#include "light_model.h"

/* Clamp a signed brightness value to 0..100%. */
static uint8_t clamp_brightness(int32_t value) {
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return (uint8_t)value;
}

/* Convert perceived brightness percent to quadratic PWM basis points. */
static uint16_t gamma_duty(uint32_t percent) { return (uint16_t)(percent * percent); }

void light_model_init(struct light_state *state, uint8_t initial_brightness) {
    state->mode = LIGHT_MODE_OFF;
    state->brightness = clamp_brightness(initial_brightness);
}

void light_model_toggle(struct light_state *state) {
    state->mode = state->mode == LIGHT_MODE_OFF ? LIGHT_MODE_NIGHTLIGHT : LIGHT_MODE_OFF;
}

void light_model_adjust(struct light_state *state, int32_t delta) {
    state->brightness = clamp_brightness((int32_t)state->brightness + delta);
    state->mode = LIGHT_MODE_NIGHTLIGHT;
}

void light_model_start_wakeup(struct light_state *state) { state->mode = LIGHT_MODE_WAKEUP; }

struct light_levels light_model_output(const struct light_state *state,
                                       uint16_t progress_permille) {
    struct light_levels levels = {0};
    if (state->mode == LIGHT_MODE_NIGHTLIGHT) {
        levels.warm_duty_bp = gamma_duty(state->brightness);
    } else if (state->mode == LIGHT_MODE_WAKEUP) {
        uint32_t progress = progress_permille > 1000 ? 1000 : progress_permille;
        uint32_t warm_percent = progress < 500 ? progress / 5 : 100;
        uint32_t cold_percent = progress < 500 ? 0 : (progress - 500) / 5;
        levels.warm_duty_bp = gamma_duty(warm_percent);
        levels.cold_duty_bp = gamma_duty(cold_percent);
    }
    return levels;
}
