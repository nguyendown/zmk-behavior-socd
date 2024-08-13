/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_socd

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct socd_input {
    bool is_pressed;
    bool is_absolute_priority;
    uint32_t position;
    struct zmk_behavior_binding binding;
};

struct behavior_socd_config {
    const bool neutral;
    const bool resume;
    const bool first_input_priority;
    const bool has_absolute_priority;
    const uint32_t absolute_priority;
    const char *behavior_dev;
};

struct behavior_socd_data {
    struct socd_input *current_input;
    struct socd_input *other_input;
    struct socd_input input_first;
    struct socd_input input_second;
};

static inline bool matches_params(const struct zmk_behavior_binding *this,
                                  const struct zmk_behavior_binding *that) {
    return this->param1 == that->param1 && this->param2 == that->param2;
}

static inline bool matches_binding(const struct zmk_behavior_binding *this,
                                   const struct zmk_behavior_binding *that) {
    return matches_params(this, that) && strcmp(this->behavior_dev, that->behavior_dev) == 0;
}

static inline void release_binding(struct socd_input *input, int64_t timestamp) {
    struct zmk_behavior_binding_event event = {
        .position = input->position,
        .timestamp = timestamp,
    };

    behavior_keymap_binding_released(&input->binding, event);
}

static inline void press_binding(struct socd_input *input, int64_t timestamp) {
    struct zmk_behavior_binding_event event = {
        .position = input->position,
        .timestamp = timestamp,
    };

    behavior_keymap_binding_pressed(&input->binding, event);
}

static int behavior_socd_init(const struct device *dev) { return 0; };

static int on_socd_binding_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    LOG_DBG("position = %d, param1 = 0x%02X.", event.position, binding->param1);

    const int64_t timestamp = event.timestamp;
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_socd_config *config = dev->config;
    const bool resume = config->resume;
    const bool neutral = config->neutral;
    const bool has_absolute_priority = config->has_absolute_priority;
    const bool first_input_priority = config->first_input_priority;
    const uint32_t absolute_priority = config->absolute_priority;
    struct behavior_socd_data *data = dev->data;
    struct socd_input *current_input = data->current_input;
    struct socd_input *other_input = data->other_input;
    bool can_press = true;

    if (current_input->is_pressed) {
        if (other_input->is_pressed) {
            return ZMK_BEHAVIOR_OPAQUE;
        }

        if (current_input->is_absolute_priority) {
            can_press = false;
        } else {
            const bool current_input_priority = !has_absolute_priority && first_input_priority;
            can_press = !(neutral || current_input_priority);

            if (!current_input_priority) {
                release_binding(current_input, timestamp);
                current_input->is_pressed = resume;

                data->current_input = other_input;
                data->other_input = current_input;
            }
        }
    } else {
        other_input = current_input;
    }

    other_input->is_pressed = true;
    other_input->position = event.position;
    other_input->binding.param1 = binding->param1;
    other_input->binding.param2 = binding->param2;

    if (can_press) {
        press_binding(other_input, timestamp);
    }

    other_input->is_absolute_priority =
        has_absolute_priority && binding->param1 == absolute_priority;

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_socd_binding_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    LOG_DBG("position = %d, param1 = 0x%02X.", event.position, binding->param1);

    const int64_t timestamp = event.timestamp;
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_socd_config *config = dev->config;
    const bool resume = config->resume;
    const bool neutral = config->neutral;
    struct behavior_socd_data *data = dev->data;
    struct socd_input *current_input = data->current_input;
    struct socd_input *other_input = data->other_input;

    if (!current_input->is_pressed) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (!other_input->is_pressed) {
        release_binding(current_input, timestamp);
        current_input->is_pressed = false;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (other_input->position == event.position && matches_params(&other_input->binding, binding)) {
        other_input->is_pressed = false;

        if (neutral) {
            press_binding(current_input, timestamp);
        }
    } else {
        if (!neutral) {
            release_binding(current_input, timestamp);
        }

        if (resume) {
            press_binding(other_input, timestamp);
        }

        current_input->is_pressed = false;
        data->current_input = other_input;
        data->other_input = current_input;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static int socd_parameter_metadata(const struct device *socd,
                                   struct behavior_parameter_metadata *param_metadata) {
    const struct behavior_socd_config *cfg = socd->config;
    struct behavior_parameter_metadata child_metadata;

    int err = behavior_get_parameter_metadata(zmk_behavior_get_binding(cfg->behavior_dev),
                                              &child_metadata);
    if (err < 0) {
        LOG_WRN("Failed to get the socd behavior parameter: %d", err);
        return err;
    }

    for (int s = 0; s < child_metadata.sets_len; s++) {
        const struct behavior_parameter_metadata_set *set = &child_metadata.sets[s];

        if (set->param2_values_len > 0) {
            return -ENOTSUP;
        }
    }

    *param_metadata = child_metadata;

    return 0;
}

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static struct behavior_driver_api behavior_socd_driver_api = {
    .binding_pressed = on_socd_binding_pressed,
    .binding_released = on_socd_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = socd_parameter_metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define _EMPTY_INPUT(inst)                                                                         \
    {                                                                                              \
        .is_pressed = false,                                                                       \
        .is_absolute_priority = false,                                                             \
        .position = 0,                                                                             \
        .binding =                                                                                 \
            {                                                                                      \
                .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(inst, bindings, 0)),         \
                .param1 = 0,                                                                       \
                .param2 = 0,                                                                       \
            },                                                                                     \
    }

#define KP_INST(inst)                                                                              \
    static const struct behavior_socd_config behavior_socd_config_##inst = {                       \
        .neutral = DT_INST_PROP(inst, neutral),                                                    \
        .resume = !DT_INST_PROP(inst, no_resume) || DT_INST_PROP(inst, neutral),                   \
        .first_input_priority =                                                                    \
            DT_INST_PROP(inst, first_input_priority) && !DT_INST_PROP(inst, neutral),              \
        .has_absolute_priority =                                                                   \
            DT_INST_NODE_HAS_PROP(inst, absolute_priority) && !DT_INST_PROP(inst, neutral),        \
        .absolute_priority = DT_INST_PROP_OR(inst, absolute_priority, 0),                          \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(inst, bindings, 0)),                 \
    };                                                                                             \
    static struct behavior_socd_data behavior_socd_data_##inst = {                                 \
        .current_input = &behavior_socd_data_##inst.input_first,                                   \
        .other_input = &behavior_socd_data_##inst.input_second,                                    \
        .input_first = _EMPTY_INPUT(inst),                                                         \
        .input_second = _EMPTY_INPUT(inst),                                                        \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(inst, behavior_socd_init, NULL, &behavior_socd_data_##inst,            \
                            &behavior_socd_config_##inst, POST_KERNEL,                             \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_socd_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
