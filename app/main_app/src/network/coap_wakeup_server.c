#include "coap_wakeup_server.h"
#include "led_control.h"
#include "logic/coap_payload.h"

#include <errno.h>
#include <openthread.h>
#include <openthread/coap.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(coap_wakeup, LOG_LEVEL_INF);

static otInstance *coap_instance;
static otCoapResource wakeup_resource;
static bool owns_coap_listener;

/* Reply to Confirmable CoAP requests and release unsent responses. */
static void reply_to_request(otMessage *request, const otMessageInfo *peer, otCoapCode code) {
    if (otCoapMessageGetType(request) != OT_COAP_TYPE_CONFIRMABLE) {
        return;
    }
    otMessage *response = otCoapNewMessage(coap_instance, NULL);
    if (response == NULL) {
        LOG_ERR("Unable to allocate CoAP response");
        return;
    }
    otError error = otCoapMessageInitResponse(response, request, OT_COAP_TYPE_ACKNOWLEDGMENT, code);
    if (error == OT_ERROR_NONE) {
        error = otCoapSendResponse(coap_instance, response, peer);
    }
    /* OpenThread owns an enqueued response; only free it when enqueueing failed. */
    if (error != OT_ERROR_NONE) {
        LOG_WRN("CoAP response failed: %d", error);
        otMessageFree(response);
    }
}

/* Validate a CoAP command and return the corresponding result. */
static void wakeup_handler(void *context, otMessage *message, const otMessageInfo *info) {
    ARG_UNUSED(context);
    otCoapCode method = otCoapMessageGetCode(message);
    if (method != OT_COAP_CODE_PUT && method != OT_COAP_CODE_POST) {
        reply_to_request(message, info, OT_COAP_CODE_METHOD_NOT_ALLOWED);
        return;
    }
    uint16_t offset = otMessageGetOffset(message);
    uint16_t total = otMessageGetLength(message);
    uint8_t data[16];
    if (total < offset || total - offset > sizeof(data)) {
        reply_to_request(message, info, OT_COAP_CODE_BAD_REQUEST);
        return;
    }
    uint16_t length = total - offset;
    int read = otMessageRead(message, offset, data, length);
    if (read < 0 || read != length) {
        reply_to_request(message, info, OT_COAP_CODE_BAD_REQUEST);
        return;
    }
    uint32_t seconds = 0;
    enum coap_payload_result result =
        coap_payload_parse(data, length, CONFIG_WAKEUP_MAX_DURATION_S, &seconds);
    if (result == COAP_PAYLOAD_INVALID) {
        reply_to_request(message, info, OT_COAP_CODE_BAD_REQUEST);
        return;
    }
    if (result == COAP_PAYLOAD_PING) {
        reply_to_request(message, info, OT_COAP_CODE_CHANGED);
        return;
    }
    int error = led_control_start_wakeup(seconds);
    if (error != 0) {
        LOG_WRN("CoAP command rejected: %d", error);
        reply_to_request(message, info, OT_COAP_CODE_SERVICE_UNAVAILABLE);
        return;
    }
    LOG_INF("Wake-up accepted: %u seconds", seconds);
    reply_to_request(message, info, OT_COAP_CODE_CHANGED);
}

int coap_wakeup_server_start(otInstance *instance) {
    if (instance == NULL) {
        return -EINVAL;
    }
    if (coap_instance != NULL) {
        return -EALREADY;
    }
    wakeup_resource = (otCoapResource){
        .mUriPath = "wakeup",
        .mContext = NULL,
        .mHandler = wakeup_handler,
        .mNext = NULL,
    };
    openthread_mutex_lock();
    otCoapAddResource(instance, &wakeup_resource);
    otError error = otCoapStart(instance, OT_DEFAULT_COAP_PORT);
    if (error != OT_ERROR_NONE && error != OT_ERROR_ALREADY) {
        otCoapRemoveResource(instance, &wakeup_resource);
        openthread_mutex_unlock();
        LOG_ERR("CoAP listener failed: %d", error);
        return -EIO;
    }
    owns_coap_listener = error == OT_ERROR_NONE;
    coap_instance = instance;
    openthread_mutex_unlock();
    return 0;
}

void coap_wakeup_server_stop(void) {
    if (coap_instance == NULL) {
        return;
    }
    openthread_mutex_lock();
    otCoapRemoveResource(coap_instance, &wakeup_resource);
    if (owns_coap_listener) {
        otCoapStop(coap_instance);
    }
    openthread_mutex_unlock();
    coap_instance = NULL;
}
