#include "ot_manager.h"

#include <errno.h>
#include <openthread.h>
#include <openthread/thread_ftd.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(ot_manager, LOG_LEVEL_INF);

static otInstance *instance;
static struct openthread_state_changed_callback role_change_callback;
static struct k_work role_work;
static struct k_work_delayable router_work;
static atomic_t partition_recovery_active;
static ot_manager_role_callback role_listener;
static void *role_listener_context;
static bool registered;
static struct ot_manager_policy role_policy;

/* Attempt Router promotion when the current role is Child. */
static void router_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!role_policy.force_router_promotion || atomic_get(&partition_recovery_active) != 0) {
        return;
    }
    openthread_mutex_lock();
    if (otThreadGetDeviceRole(instance) == OT_DEVICE_ROLE_CHILD) {
        otError eligibility = otThreadSetRouterEligible(instance, true);
        otError promotion =
            eligibility == OT_ERROR_NONE ? otThreadBecomeRouter(instance) : eligibility;
        LOG_INF("Router promotion: eligibility=%d promotion=%d", eligibility, promotion);
    }
    openthread_mutex_unlock();
}

/* Handle current role and apply partition policy outside the OpenThread callback. */
static void role_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    openthread_mutex_lock();
    otDeviceRole role = otThreadGetDeviceRole(instance);
    openthread_mutex_unlock();

    if (role_listener != NULL) {
        role_listener(role, role_listener_context);
    }

    switch (role) {
    case OT_DEVICE_ROLE_CHILD:
        LOG_INF("Thread role: CHILD");
        atomic_clear(&partition_recovery_active);
        if (role_policy.force_router_promotion) {
            k_work_reschedule(&router_work, K_SECONDS(role_policy.router_promotion_delay_s));
        }
        break;
    case OT_DEVICE_ROLE_ROUTER:
        LOG_INF("Thread role: ROUTER");
        atomic_clear(&partition_recovery_active);
        k_work_cancel_delayable(&router_work);
        break;
    case OT_DEVICE_ROLE_LEADER:
        LOG_WRN("Thread role: LEADER");
        k_work_cancel_delayable(&router_work);
        if (role_policy.detach_on_leader && atomic_cas(&partition_recovery_active, 0, 1)) {
            openthread_mutex_lock();
            otError eligible = otThreadSetRouterEligible(instance, false);
            otError detach = otThreadBecomeDetached(instance);
            openthread_mutex_unlock();
            LOG_WRN("Partition recovery: router eligible=%d detach=%d", eligible, detach);
            if (detach != OT_ERROR_NONE) {
                atomic_clear(&partition_recovery_active);
            }
        }
        break;
    case OT_DEVICE_ROLE_DETACHED:
        LOG_WRN("Thread role: DETACHED; waiting for OpenThread re-attach");
        k_work_cancel_delayable(&router_work);
        break;
    case OT_DEVICE_ROLE_DISABLED:
        LOG_WRN("Thread role: DISABLED");
        k_work_cancel_delayable(&router_work);
        break;
    }
}

/* Defer role/partition notifications to a Zephyr work item. */
static void thread_state_changed(otChangedFlags flags, void *context) {
    ARG_UNUSED(context);
    if ((flags & (OT_CHANGED_THREAD_ROLE | OT_CHANGED_THREAD_PARTITION_ID)) != 0) {
        k_work_submit(&role_work);
    }
}

int ot_manager_init(const struct ot_manager_policy *policy, ot_manager_role_callback callback,
                    void *context) {
    if (registered) {
        return -EALREADY;
    }
    if (policy == NULL ||
        (policy->force_router_promotion && policy->router_promotion_delay_s == 0)) {
        return -EINVAL;
    }
    role_policy = *policy;
    instance = openthread_get_default_instance();
    if (instance == NULL) {
        return -ENODEV;
    }
    role_listener = callback;
    role_listener_context = context;
    atomic_clear(&partition_recovery_active);
    k_work_init(&role_work, role_work_handler);
    k_work_init_delayable(&router_work, router_work_handler);
    role_change_callback = (struct openthread_state_changed_callback){
        .otCallback = thread_state_changed,
        .user_data = NULL,
    };
    int error = openthread_state_changed_callback_register(&role_change_callback);
    if (error != 0) {
        return error;
    }
    registered = true;
    return 0;
}

int ot_manager_start(void) {
    if (!registered) {
        return -EAGAIN;
    }
    openthread_mutex_lock();
    otDeviceRole role = otThreadGetDeviceRole(instance);
    if (role_policy.detach_on_leader &&
        (role == OT_DEVICE_ROLE_DISABLED || role == OT_DEVICE_ROLE_DETACHED)) {
        otError err = otThreadSetRouterEligible(instance, false);
        if (err != OT_ERROR_NONE) {
            openthread_mutex_unlock();
            LOG_ERR("Unable to prohibit standalone partition at startup: %d", err);
            return -EIO;
        }
    }
    if (role == OT_DEVICE_ROLE_DISABLED) {
        otError err = otThreadSetEnabled(instance, true);
        if (err != OT_ERROR_NONE) {
            openthread_mutex_unlock();
            LOG_ERR("Thread start failed: %d", err);
            return -EIO;
        }
    }
    openthread_mutex_unlock();
    k_work_submit(&role_work);
    return 0;
}

void ot_manager_deinit(void) {
    if (!registered) {
        return;
    }
    openthread_state_changed_callback_unregister(&role_change_callback);
    k_work_cancel_delayable(&router_work);
    registered = false;
}

otInstance *ot_manager_instance(void) { return instance; }
