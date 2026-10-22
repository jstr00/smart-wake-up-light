#ifndef COAP_PAYLOAD_H
#define COAP_PAYLOAD_H

#include <stddef.h>
#include <stdint.h>

enum coap_payload_result { COAP_PAYLOAD_INVALID, COAP_PAYLOAD_PING, COAP_PAYLOAD_WAKEUP };

/** Parse exact ASCII "ping" or unsigned decimal seconds in 1..max_seconds. On COAP_PAYLOAD_WAKEUP
 * write the duration to seconds; otherwise do not use the output. */
enum coap_payload_result coap_payload_parse(const uint8_t *data, size_t length,
                                            uint32_t max_seconds, uint32_t *seconds);

#endif
