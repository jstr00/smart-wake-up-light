#include "srp_service.h"

#include <errno.h>
#include <openthread.h>
#include <openthread/dns.h>
#include <openthread/srp_client.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(srp_service, LOG_LEVEL_INF);

static otInstance *srp_instance;
static otSrpClientService srp_registration;
static otDnsTxtEntry txt_entries[3];
static const uint8_t protocol[] = "coap";
static const struct srp_service_config *active_config;

/* Report whether SRP automatic startup found a server. */
static void auto_start_callback(const otSockAddr *server, void *context) {
    ARG_UNUSED(context);
    if (server == NULL) {
        LOG_WRN("SRP server not available");
    } else {
        LOG_INF("SRP client started");
    }
}

/* Log SRP registration results for this service. */
static void registration_callback(otError error, const otSrpClientHostInfo *host_info,
                                  const otSrpClientService *services,
                                  const otSrpClientService *removed_services, void *context) {
    ARG_UNUSED(host_info);
    ARG_UNUSED(removed_services);
    ARG_UNUSED(context);
    if (error != OT_ERROR_NONE) {
        LOG_WRN("SRP registration failed: %d", error);
        return;
    }
    for (const otSrpClientService *service = services; service != NULL; service = service->mNext) {
        if (service == &srp_registration &&
            service->mState == OT_SRP_CLIENT_ITEM_STATE_REGISTERED) {
            LOG_INF("SRP registered: %s.%s", active_config->instance_name,
                    active_config->service_type);
            break;
        }
    }
}

int srp_service_register(otInstance *instance, const struct srp_service_config *config) {
    if (instance == NULL || config == NULL || config->host_name == NULL ||
        config->instance_name == NULL || config->service_type == NULL ||
        config->resource_path == NULL || config->port == 0) {
        return -EINVAL;
    }
    if (srp_instance != NULL) {
        return -EALREADY;
    }
    active_config = config;
    txt_entries[0] = (otDnsTxtEntry){
        .mKey = "path",
        .mValue = (const uint8_t *)config->resource_path,
        .mValueLength = (uint16_t)strlen(config->resource_path),
    };
    txt_entries[1] = (otDnsTxtEntry){
        .mKey = "proto",
        .mValue = protocol,
        .mValueLength = sizeof(protocol) - 1,
    };
    size_t entry_count = 2;
    if (config->version != NULL) {
        txt_entries[2] = (otDnsTxtEntry){
            .mKey = "version",
            .mValue = (const uint8_t *)config->version,
            .mValueLength = (uint16_t)strlen(config->version),
        };
        entry_count++;
    }
    srp_registration = (otSrpClientService){
        .mName = config->service_type,
        .mInstanceName = config->instance_name,
        .mTxtEntries = txt_entries,
        .mNumTxtEntries = (uint8_t)entry_count,
        .mPort = config->port,
    };
    openthread_mutex_lock();
    otSrpClientSetCallback(instance, registration_callback, NULL);
    otError err = otSrpClientSetHostName(instance, config->host_name);
    if (err == OT_ERROR_NONE) {
        err = otSrpClientEnableAutoHostAddress(instance);
    }
    if (err == OT_ERROR_NONE) {
        err = otSrpClientAddService(instance, &srp_registration);
    }
    if (err == OT_ERROR_NONE) {
        otSrpClientEnableAutoStartMode(instance, auto_start_callback, NULL);
    }
    openthread_mutex_unlock();
    if (err != OT_ERROR_NONE) {
        LOG_ERR("SRP setup failed: %d", err);
        return -EIO;
    }
    srp_instance = instance;
    return 0;
}
