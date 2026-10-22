#ifndef OT_MANAGER_H
#define OT_MANAGER_H

#include <openthread/instance.h>
#include <openthread/thread.h>
#include <stdbool.h>
#include <stdint.h>

typedef void (*ot_manager_role_callback)(otDeviceRole role, void *context);

struct ot_manager_policy {
    uint16_t router_promotion_delay_s;
    bool force_router_promotion;
    bool detach_on_leader;
};

/** Register role callbacks and apply a copied runtime Thread policy. The caller retains callback
 * context lifetime. Return 0 or negative errno. */
int ot_manager_init(const struct ot_manager_policy *policy, ot_manager_role_callback callback,
                    void *context);
/** Apply startup role eligibility policy and start Thread if disabled; return 0 or negative errno.
 */
int ot_manager_start(void);
/** Unregister the role callback and cancel Router promotion work; does not disable Thread. */
void ot_manager_deinit(void);
/** Get the OpenThread singleton instance used by this manager. */
otInstance *ot_manager_instance(void);

#endif
