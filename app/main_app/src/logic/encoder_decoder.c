#include "encoder_decoder.h"

void encoder_decoder_init(struct encoder_decoder *decoder, uint8_t initial_state) {
    decoder->previous = initial_state & 3;
    decoder->accumulator = 0;
}

int8_t encoder_decoder_update(struct encoder_decoder *decoder, uint8_t next_state) {
    static const int8_t transitions[16] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
    uint8_t next = next_state & 3;
    if ((decoder->previous ^ next) == 3) {
        decoder->accumulator = 0;
    } else {
        decoder->accumulator += transitions[(decoder->previous << 2) | next];
    }
    decoder->previous = next;
    if (decoder->accumulator >= 4) {
        decoder->accumulator = 0;
        return 1;
    }
    if (decoder->accumulator <= -4) {
        decoder->accumulator = 0;
        return -1;
    }
    return 0;
}
