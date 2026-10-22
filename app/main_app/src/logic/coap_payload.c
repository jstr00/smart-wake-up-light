#include "coap_payload.h"

#include <limits.h>

/* Identify the exact ping payload. */
static int is_ping(const uint8_t *data, size_t length) {
    return length == 4 && data[0] == 'p' && data[1] == 'i' && data[2] == 'n' && data[3] == 'g';
}

enum coap_payload_result coap_payload_parse(const uint8_t *data, size_t length,
                                            uint32_t max_seconds, uint32_t *seconds) {
    if (data == NULL || seconds == NULL || length == 0) {
        return COAP_PAYLOAD_INVALID;
    }
    if (is_ping(data, length)) {
        *seconds = 0;
        return COAP_PAYLOAD_PING;
    }
    uint32_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        if (data[i] < '0' || data[i] > '9') {
            return COAP_PAYLOAD_INVALID;
        }
        uint32_t digit = data[i] - '0';
        if (value > (UINT32_MAX - digit) / 10) {
            return COAP_PAYLOAD_INVALID;
        }
        value = value * 10 + digit;
    }
    if (value == 0 || value > max_seconds) {
        return COAP_PAYLOAD_INVALID;
    }
    *seconds = value;
    return COAP_PAYLOAD_WAKEUP;
}
