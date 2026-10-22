#ifndef COAP_WAKEUP_SERVER_H
#define COAP_WAKEUP_SERVER_H

#include <openthread/instance.h>

/** Register /wakeup on an OpenThread instance and start or reuse its CoAP listener; return 0 or
 * negative errno. */
int coap_wakeup_server_start(otInstance *instance);
/** Unregister the endpoint, stopping the listener only if this module started it. */
void coap_wakeup_server_stop(void);

#endif
