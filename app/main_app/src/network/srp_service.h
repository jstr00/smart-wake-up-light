#ifndef SRP_SERVICE_H
#define SRP_SERVICE_H

#include <openthread/instance.h>
#include <stdint.h>

struct srp_service_config {
    const char *host_name;
    const char *instance_name;
    const char *service_type;
    const char *resource_path;
    const char *version;
    uint16_t port;
};

/** Register one SRP service. The instance and config string storage must remain valid for the
 * firmware lifetime; return 0 or negative errno. */
int srp_service_register(otInstance *instance, const struct srp_service_config *config);

#endif
