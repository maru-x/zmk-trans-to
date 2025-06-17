/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_trans_to

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>


LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_trans_to_config {
    uint32_t timeout_ms;
    uint8_t return_layer;
};

struct behavior_trans_to_data {
    struct k_work_delayable delayed_work;
    const struct device *dev; // Store dev pointer for work callback
    bool timer_active; // Flag to prevent double activation
};

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static void layer_switch_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct behavior_trans_to_data *data = CONTAINER_OF(dwork, struct behavior_trans_to_data, delayed_work);
    const struct device *dev = data->dev;
    const struct behavior_trans_to_config *config = dev->config;

    LOG_DBG("TRT: Work handler executed, switching to layer %u", config->return_layer);
    
    // Clear the timer active flag
    data->timer_active = false;
    
    int err = zmk_keymap_layer_to(config->return_layer);
    if (err) {
        LOG_ERR("TRT: Failed to switch to layer %u: %d", config->return_layer, err);
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = binding->behavior_dev;
    struct behavior_trans_to_data *data = dev->data;
    
    LOG_DBG("TRT: Key pressed, canceling any pending work");
    
    // Cancel any pending delayed work and clear the timer active flag
    if (data->timer_active) {
        int ret = k_work_cancel_delayable(&data->delayed_work);
        if (ret == 0) {
            LOG_DBG("TRT: Pending work was canceled");
        } else if (ret > 0) {
            LOG_DBG("TRT: Work was already running, couldn't cancel");
        }
        data->timer_active = false;
    }
    
    return ZMK_BEHAVIOR_TRANSPARENT;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = binding->behavior_dev;
    LOG_DBG("TRT: on_keymap_binding_released for %s. dev=%p", dev->name, (void*)dev);
    if (!dev) {
        LOG_ERR("TRT: dev is NULL!");
        return ZMK_BEHAVIOR_TRANSPARENT;
    }
    
    const struct behavior_trans_to_config *config = dev->config;
    LOG_DBG("TRT: config=%p, timeout_ms=%u, return_layer=%u", (void*)config, 
            config ? config->timeout_ms : 0, config ? config->return_layer : 0);
    if (!config) {
        LOG_ERR("TRT: config is NULL!");
        return ZMK_BEHAVIOR_TRANSPARENT;
    }
    
    struct behavior_trans_to_data *data = dev->data;
    LOG_DBG("TRT: data=%p", (void*)data);
    if (!data) {
        LOG_ERR("TRT: data is NULL!");
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    if (config->timeout_ms > 0) {
        // Prevent double activation
        if (data->timer_active) {
            LOG_DBG("TRT: Timer already active, canceling previous work");
            k_work_cancel_delayable(&data->delayed_work);
        }
        
        LOG_DBG("TRT: About to schedule delayed work for %s. timeout=%ums", dev->name, config->timeout_ms);
        
        // Set the timer active flag before scheduling
        data->timer_active = true;
        
        int ret = k_work_schedule(&data->delayed_work, K_MSEC(config->timeout_ms));
        if (ret < 0) {
            LOG_ERR("TRT: Failed to schedule delayed work: %d", ret);
            data->timer_active = false;
            return ret;
        }
        
        LOG_DBG("TRT: Delayed work scheduled for layer %u, timeout %u ms on %s", 
                config->return_layer, config->timeout_ms, dev->name);
    } else {
        LOG_DBG("TRT: Delayed work not scheduled for %s as timeout_ms is 0.", dev->name);
    }
    
    return ZMK_BEHAVIOR_TRANSPARENT;
}

static int behavior_trans_to_init(const struct device *dev) {
    struct behavior_trans_to_data *data = dev->data;
    const struct behavior_trans_to_config *config = dev->config;

    data->dev = dev; // Store dev pointer
    data->timer_active = false; // Initialize timer active flag

    LOG_DBG("TRT: Initializing behavior_trans_to '%s' with timeout %ums, return_layer %u",
            dev->name, config->timeout_ms, config->return_layer);

    k_work_init_delayable(&data->delayed_work, layer_switch_work_handler);
    
    return 0;
}

static const struct behavior_driver_api behavior_trans_to_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define TRT_INST(n)                                                                                \
    static struct behavior_trans_to_data behavior_trans_to_data_##n;                             \
    static const struct behavior_trans_to_config behavior_trans_to_config_##n = {               \
        .timeout_ms = DT_INST_PROP(n, timeout_ms),                                               \
        .return_layer = DT_INST_PROP(n, return_layer),                                           \
    };                                                                                            \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_trans_to_init, NULL,                                    \
                            &behavior_trans_to_data_##n,                                        \
                            &behavior_trans_to_config_##n, POST_KERNEL,                         \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                \
                            &behavior_trans_to_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TRT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
