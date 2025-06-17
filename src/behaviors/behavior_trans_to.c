/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_trans_to

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h> // Added for k_timer

#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>    // Added for zmk_keymap_layer_activate
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>


LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_trans_to_config {
    uint32_t timeout_ms;
    uint8_t return_layer;
};

struct behavior_trans_to_data {
    struct k_timer timer;
    const struct device *dev; // Store dev pointer for timer callback
};

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static void mcb_timer_expiry_handler(struct k_timer *timer_id) {
    struct behavior_trans_to_data *data = CONTAINER_OF(timer_id, struct behavior_trans_to_data, timer);
    const struct device *dev = data->dev;
    const struct behavior_trans_to_config *config = dev->config;

    LOG_DBG("Transparent back: Timer expired, switching to layer %u", config->return_layer);
    int err = zmk_keymap_layer_to(config->return_layer);
    if (err) {
        LOG_ERR("Failed to switch to layer %u: %d", config->return_layer, err);
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_TRANSPARENT;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = binding->behavior_dev;
    LOG_DBG("TRT: on_keymap_binding_released for %s. dev=%p", dev->name, (void*)dev); // 追加
    if (!dev) {
        LOG_ERR("TRT: dev is NULL!");
        return ZMK_BEHAVIOR_TRANSPARENT;
    }
    const struct behavior_trans_to_config *config = dev->config;
    LOG_DBG("TRT: config=%p, timeout_ms=%u, return_layer=%u", (void*)config, config ? config->timeout_ms : 0, config ? config->return_layer : 0); // 追加
    if (!config) {
        LOG_ERR("TRT: config is NULL!");
        return ZMK_BEHAVIOR_TRANSPARENT;
    }
    struct behavior_trans_to_data *data = dev->data;
    LOG_DBG("TRT: data=%p", (void*)data); // 追加
    if (!data) {
        LOG_ERR("TRT: data is NULL!");
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    if (config->timeout_ms > 0) {
        LOG_DBG("TRT: About to start timer for %s. timer_ptr=%p", dev->name, (void*)&data->timer); // 追加
        k_timer_start(&data->timer, K_MSEC(config->timeout_ms), K_NO_WAIT);
        LOG_DBG("Timer started for layer %u, timeout %u ms on %s", config->return_layer, config->timeout_ms, dev->name);
    } else {
        LOG_DBG("Timer not started for %s as timeout_ms is 0.", dev->name);
    }
    
    return ZMK_BEHAVIOR_TRANSPARENT;
}

static int behavior_trans_to_init(const struct device *dev) {
    struct behavior_trans_to_data *data = dev->data;
    const struct behavior_trans_to_config *config = dev->config;

    data->dev = dev; // Store dev pointer

    LOG_DBG("Initializing behavior_trans_to '%s' with timeout %ums, return_layer %u",
              dev->name, config->timeout_ms, config->return_layer);

    k_timer_init(&data->timer, mcb_timer_expiry_handler, NULL);
    return 0;
}

static const struct behavior_driver_api behavior_trans_to_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define TRT_INST(n)                                                                                \
    static struct behavior_trans_to_data behavior_trans_to_data_##n;                \
    static const struct behavior_trans_to_config behavior_trans_to_config_##n = {  \
        .timeout_ms = DT_INST_PROP(n, timeout_ms),                                                 \
        .return_layer = DT_INST_PROP(n, return_layer),                                             \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_trans_to_init, NULL,                                     \
                            &behavior_trans_to_data_##n,                                       \
                            &behavior_trans_to_config_##n, POST_KERNEL,                        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                 \
                            &behavior_trans_to_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TRT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
