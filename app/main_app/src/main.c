#include <openthread/coap.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "encoder.h"
#include "led_control.h"
#include "network/coap_wakeup_server.h"
#include "network/ot_manager.h"
#include "network/srp_service.h"
#include "status_led.h"

LOG_MODULE_REGISTER(wake_up_main, LOG_LEVEL_INF);

static const struct ot_manager_policy thread_policy = {
    .router_promotion_delay_s = CONFIG_WAKEUP_ROUTER_PROMOTION_DELAY_S,
    .force_router_promotion = true,
    .detach_on_leader = true,
};

static const struct srp_service_config srp_config = {
    .host_name = CONFIG_WAKEUP_SRP_HOST_NAME,
    .instance_name = CONFIG_WAKEUP_SRP_INSTANCE_NAME,
    .service_type = "_smart-wakeup._udp",
    .resource_path = "/wakeup",
    .version = "1",
    .port = OT_DEFAULT_COAP_PORT,
};

/* Translate Thread roles into the indicator color. */
static void on_thread_role(otDeviceRole role, void *context) {
    ARG_UNUSED(context);
    switch (role) {
    case OT_DEVICE_ROLE_ROUTER:
        status_led_set(0, 0, 0);
        break;
    case OT_DEVICE_ROLE_CHILD:
        status_led_set(0, 20, 0);
        break;
    case OT_DEVICE_ROLE_DETACHED:
        status_led_set(20, 15, 0);
        break;
    case OT_DEVICE_ROLE_LEADER:
    case OT_DEVICE_ROLE_DISABLED:
        status_led_set(20, 0, 0);
        break;
    }
}

/* Initialize device services and start the Thread application. */
int main(void) {
    int error = led_control_init();
    if (error != 0) {
        LOG_ERR("Light control initialization failed: %d", error);
        return error;
    }
    error = encoder_init();
    if (error != 0) {
        LOG_ERR("Encoder initialization failed: %d", error);
        return error;
    }
    status_led_init();
    error = ot_manager_init(&thread_policy, on_thread_role, NULL);
    if (error != 0) {
        LOG_ERR("OpenThread initialization failed: %d", error);
        return error;
    }
    error = coap_wakeup_server_start(ot_manager_instance());
    if (error != 0) {
        LOG_ERR("CoAP initialization failed: %d", error);
        ot_manager_deinit();
        return error;
    }
    error = srp_service_register(ot_manager_instance(), &srp_config);
    if (error != 0) {
        LOG_ERR("SRP initialization failed: %d", error);
        coap_wakeup_server_stop();
        ot_manager_deinit();
        return error;
    }
    error = ot_manager_start();
    if (error != 0) {
        LOG_ERR("Thread start failed: %d", error);
        coap_wakeup_server_stop();
        ot_manager_deinit();
        return error;
    }
    LOG_INF("Smart wake-up light initialized");
    return 0;
}
