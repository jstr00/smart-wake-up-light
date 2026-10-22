#ifndef LIGHT_MODEL_H
#define LIGHT_MODEL_H

#include <stdint.h>

enum light_mode { LIGHT_MODE_OFF, LIGHT_MODE_NIGHTLIGHT, LIGHT_MODE_WAKEUP };

struct light_state {
    enum light_mode mode;
    uint8_t brightness;
};

struct light_levels {
    uint16_t warm_duty_bp;
    uint16_t cold_duty_bp;
};

/** Initialize OFF mode with warm-white nightlight brightness clamped to 0..100%. */
void light_model_init(struct light_state *state, uint8_t initial_brightness);
/** Toggle OFF/NIGHTLIGHT; any active wake-up is stopped and switched OFF. */
void light_model_toggle(struct light_state *state);
/** Apply a signed brightness delta, clamped to 0..100%; enters NIGHTLIGHT. */
void light_model_adjust(struct light_state *state, int32_t delta);
/** Enter WAKEUP mode, resetting animation progress in the caller. */
void light_model_start_wakeup(struct light_state *state);
/** Calculate warm/cold PWM basis points (0..10000) for the current mode and wake-up progress in
 * permille (0..1000). */
struct light_levels light_model_output(const struct light_state *state, uint16_t progress_permille);

#endif
