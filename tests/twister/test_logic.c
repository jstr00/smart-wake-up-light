#include <zephyr/ztest.h>
#include <stdint.h>
#include <string.h>
#include "light_model.h"
#include "coap_payload.h"
#include "encoder_decoder.h"

static void assert_levels(const struct light_state *state, uint16_t progress,
                          uint16_t warm, uint16_t cold)
{
    struct light_levels levels = light_model_output(state, progress);
    zassert_equal(levels.warm_duty_bp, warm);
    zassert_equal(levels.cold_duty_bp, cold);
}

static void assert_payload(const char *input, uint32_t max_seconds,
                           enum coap_payload_result expected, uint32_t duration)
{
    uint32_t parsed = UINT32_MAX;
    enum coap_payload_result result = coap_payload_parse(
        (const uint8_t *)input, strlen(input), max_seconds, &parsed);
    zassert_equal(result, expected);
    if (expected == COAP_PAYLOAD_WAKEUP) {
        zassert_equal(parsed, duration);
    } else if (expected == COAP_PAYLOAD_PING) {
        zassert_equal(parsed, 0);
    } else {
        zassert_equal(parsed, UINT32_MAX);
    }
}

static int32_t encoder_turn(struct encoder_decoder *decoder, const uint8_t states[4])
{
    int32_t result = 0;
    for (size_t i = 0; i < 4; ++i) {
        result += encoder_decoder_update(decoder, states[i]);
    }
    return result;
}

ZTEST(smart_wakeup_unit, ut_light_001_initial_brightness_clamped)
{
    struct light_state state;
    light_model_init(&state, 255);
    zassert_equal(state.mode, LIGHT_MODE_OFF);
    zassert_equal(state.brightness, 100);
    light_model_init(&state, 0);
    zassert_equal(state.brightness, 0);
}

ZTEST(smart_wakeup_unit, ut_light_002_toggle_off_nightlight)
{
    struct light_state state;
    light_model_init(&state, 10);
    light_model_toggle(&state);
    zassert_equal(state.mode, LIGHT_MODE_NIGHTLIGHT);
    light_model_toggle(&state);
    zassert_equal(state.mode, LIGHT_MODE_OFF);
    light_model_toggle(&state);
    zassert_equal(state.mode, LIGHT_MODE_NIGHTLIGHT);
    zassert_equal(state.brightness, 10);
}

ZTEST(smart_wakeup_unit, ut_light_003_press_during_wakeup_turns_off)
{
    struct light_state state;
    light_model_init(&state, 10);
    light_model_start_wakeup(&state);
    light_model_toggle(&state);
    zassert_equal(state.mode, LIGHT_MODE_OFF);
    assert_levels(&state, 1000, 0, 0);
}

ZTEST(smart_wakeup_unit, ut_light_004_adjust_clamps_and_selects_nightlight)
{
    struct light_state state;
    light_model_init(&state, 10);
    light_model_start_wakeup(&state);
    light_model_adjust(&state, 5);
    zassert_equal(state.mode, LIGHT_MODE_NIGHTLIGHT);
    zassert_equal(state.brightness, 15);
    light_model_adjust(&state, 127);
    zassert_equal(state.brightness, 100);
    light_model_adjust(&state, -127);
    zassert_equal(state.brightness, 0);
}

ZTEST(smart_wakeup_unit, ut_light_005_off_and_nightlight_outputs)
{
    struct light_state state;
    light_model_init(&state, 10);
    assert_levels(&state, 500, 0, 0);
    light_model_toggle(&state);
    assert_levels(&state, 0, 100, 0);
    light_model_adjust(&state, 90);
    assert_levels(&state, 0, 10000, 0);
}

ZTEST(smart_wakeup_unit, ut_light_006_warm_ramp_first_half)
{
    struct light_state state;
    light_model_init(&state, 10);
    light_model_start_wakeup(&state);
    assert_levels(&state, 0, 0, 0);
    assert_levels(&state, 250, 2500, 0);
    assert_levels(&state, 499, 9801, 0);
    assert_levels(&state, 500, 10000, 0);
}

ZTEST(smart_wakeup_unit, ut_light_007_cold_ramp_second_half)
{
    struct light_state state;
    light_model_init(&state, 10);
    light_model_start_wakeup(&state);
    assert_levels(&state, 500, 10000, 0);
    assert_levels(&state, 750, 10000, 2500);
    assert_levels(&state, 1000, 10000, 10000);
    assert_levels(&state, 2000, 10000, 10000);
}

ZTEST(smart_wakeup_unit, ut_light_008_pwm_curves_monotonic)
{
    struct light_state state;
    light_model_init(&state, 0);
    light_model_start_wakeup(&state);
    uint16_t previous_warm = 0;
    uint16_t previous_cold = 0;
    for (uint16_t progress = 0; progress <= 1000; ++progress) {
        struct light_levels levels = light_model_output(&state, progress);
        zassert_true(levels.warm_duty_bp >= previous_warm);
        zassert_true(levels.cold_duty_bp >= previous_cold);
        zassert_true(levels.warm_duty_bp <= 10000);
        zassert_true(levels.cold_duty_bp <= 10000);
        if (progress >= 500) {
            zassert_equal(levels.warm_duty_bp, 10000);
        }
        previous_warm = levels.warm_duty_bp;
        previous_cold = levels.cold_duty_bp;
    }
}

ZTEST(smart_wakeup_unit, ut_coap_001_ping_exact)
{
    assert_payload("ping", 3600, COAP_PAYLOAD_PING, 0);
    assert_payload("PING", 3600, COAP_PAYLOAD_INVALID, 0);
    assert_payload("ping ", 3600, COAP_PAYLOAD_INVALID, 0);
}

ZTEST(smart_wakeup_unit, ut_coap_002_duration_boundaries)
{
    assert_payload("1", 3600, COAP_PAYLOAD_WAKEUP, 1);
    assert_payload("0030", 3600, COAP_PAYLOAD_WAKEUP, 30);
    assert_payload("3600", 3600, COAP_PAYLOAD_WAKEUP, 3600);
    assert_payload("0", 3600, COAP_PAYLOAD_INVALID, 0);
    assert_payload("3601", 3600, COAP_PAYLOAD_INVALID, 0);
    assert_payload("", 3600, COAP_PAYLOAD_INVALID, 0);
}

ZTEST(smart_wakeup_unit, ut_coap_003_reject_invalid_formats)
{
    const char *invalid[] = {"-1", "+1", " 2", "2 ", "1.5", "a"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert_payload(invalid[i], 3600, COAP_PAYLOAD_INVALID, 0);
    }
}

ZTEST(smart_wakeup_unit, ut_coap_004_reject_integer_overflow)
{
    assert_payload("4294967296", UINT32_MAX, COAP_PAYLOAD_INVALID, 0);
    assert_payload("99999999999999999999999", UINT32_MAX, COAP_PAYLOAD_INVALID, 0);
}

ZTEST(smart_wakeup_unit, ut_coap_005_reject_null_and_embedded_nul)
{
    uint32_t value = UINT32_MAX;
    const uint8_t embedded[] = {'3', '0', 0, '0'};
    zassert_equal(coap_payload_parse(NULL, 1, 3600, &value), COAP_PAYLOAD_INVALID);
    zassert_equal(coap_payload_parse((const uint8_t *)"1", 1, 3600, NULL), COAP_PAYLOAD_INVALID);
    zassert_equal(coap_payload_parse(embedded, sizeof(embedded), 3600, &value),
                  COAP_PAYLOAD_INVALID);
    zassert_equal(value, UINT32_MAX);
}

ZTEST(smart_wakeup_unit, ut_coap_006_custom_duration_limit)
{
    assert_payload("2", 1, COAP_PAYLOAD_INVALID, 0);
    assert_payload("1", 1, COAP_PAYLOAD_WAKEUP, 1);
    assert_payload("1", 0, COAP_PAYLOAD_INVALID, 0);
    assert_payload("4294967295", UINT32_MAX, COAP_PAYLOAD_WAKEUP, UINT32_MAX);
}

ZTEST(smart_wakeup_unit, ut_encoder_001_clockwise_detent)
{
    struct encoder_decoder decoder;
    const uint8_t clockwise[4] = {1, 3, 2, 0};
    encoder_decoder_init(&decoder, 0);
    zassert_equal(encoder_turn(&decoder, clockwise), 1);
}

ZTEST(smart_wakeup_unit, ut_encoder_002_counterclockwise_detent)
{
    struct encoder_decoder decoder;
    const uint8_t counterclockwise[4] = {2, 3, 1, 0};
    encoder_decoder_init(&decoder, 0);
    zassert_equal(encoder_turn(&decoder, counterclockwise), -1);
}

ZTEST(smart_wakeup_unit, ut_encoder_003_invalid_transition_resets)
{
    struct encoder_decoder decoder;
    encoder_decoder_init(&decoder, 0);
    zassert_equal(encoder_decoder_update(&decoder, 1), 0);
    zassert_equal(encoder_decoder_update(&decoder, 2), 0);
    zassert_equal(decoder.accumulator, 0);
    zassert_equal(encoder_decoder_update(&decoder, 0), 0);
    zassert_equal(encoder_decoder_update(&decoder, 1), 0);
    zassert_equal(encoder_decoder_update(&decoder, 3), 0);
    zassert_equal(encoder_decoder_update(&decoder, 2), 1);
}

ZTEST(smart_wakeup_unit, ut_encoder_004_bounce_does_not_add_detent)
{
    struct encoder_decoder decoder;
    encoder_decoder_init(&decoder, 0);
    const uint8_t bounce[] = {1, 0, 1, 0, 1, 3, 2, 0};
    int32_t steps = 0;
    for (size_t i = 0; i < sizeof(bounce); ++i) {
        steps += encoder_decoder_update(&decoder, bounce[i]);
    }
    zassert_equal(steps, 1);
}

ZTEST(smart_wakeup_unit, ut_encoder_005_partial_turn_reversal)
{
    struct encoder_decoder decoder;
    encoder_decoder_init(&decoder, 0);
    const uint8_t reversed[] = {1, 3, 1, 0};
    int32_t steps = 0;
    for (size_t i = 0; i < sizeof(reversed); ++i) {
        steps += encoder_decoder_update(&decoder, reversed[i]);
    }
    zassert_equal(steps, 0);
    zassert_equal(decoder.accumulator, 0);
}

ZTEST(smart_wakeup_unit, ut_encoder_006_repeated_state_ignored)
{
    struct encoder_decoder decoder;
    encoder_decoder_init(&decoder, 0);
    for (int i = 0; i < 1000; ++i) {
        zassert_equal(encoder_decoder_update(&decoder, 0), 0);
    }
    zassert_equal(decoder.accumulator, 0);
}

ZTEST(smart_wakeup_unit, ut_encoder_007_fast_continuous_turns)
{
    struct encoder_decoder decoder;
    const uint8_t clockwise[4] = {1, 3, 2, 0};
    encoder_decoder_init(&decoder, 0);
    int32_t steps = 0;
    for (int i = 0; i < 10000; ++i) {
        steps += encoder_turn(&decoder, clockwise);
    }
    zassert_equal(steps, 10000);
}

ZTEST_SUITE(smart_wakeup_unit, NULL, NULL, NULL, NULL, NULL);
