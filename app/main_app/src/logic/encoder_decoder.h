#ifndef ENCODER_DECODER_H
#define ENCODER_DECODER_H

#include <stdint.h>

struct encoder_decoder {
    uint8_t previous;
    int8_t accumulator;
};

/** Reset the quadrature decoder using the initial two-bit GPIO state. */
void encoder_decoder_init(struct encoder_decoder *decoder, uint8_t initial_state);
/** Consume the next two-bit quadrature state and return -1, 0 or +1 completed detents. */
int8_t encoder_decoder_update(struct encoder_decoder *decoder, uint8_t next_state);

#endif
