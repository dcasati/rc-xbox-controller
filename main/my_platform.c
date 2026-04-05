// SPDX-License-Identifier: MIT
// RC Xbox Controller — Bluepad32 Custom Platform
//
// Maps Xbox Controller inputs to RC car functions:
//   LT (brake)    → drive forward (0-1023)
//   RT (throttle) → drive backward (0-1023)
//   Left stick X  → steering (-512 to 511)
//   Y button      → toggle headlights

#include <string.h>

#include <uni.h>

#include "led.h"
#include "motor_control.h"
#include "steering.h"

typedef struct my_platform_instance_s {
    uni_gamepad_seat_t gamepad_seat;
} my_platform_instance_t;

static my_platform_instance_t* get_my_platform_instance(uni_hid_device_t* d);

//
// Platform callbacks
//

static void my_platform_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    logi("rc-xbox-controller: init()\n");

    // Initialize hardware
    motor_control_init();
    steering_init();
    led_init();
}

static void my_platform_on_init_complete(void) {
    logi("rc-xbox-controller: on_init_complete()\n");

    // Start scanning for controllers
    uni_bt_start_scanning_and_autoconnect_unsafe();
    uni_bt_allow_incoming_connections(true);

    // Clear stored keys on boot so pairing is fresh
    uni_bt_del_keys_unsafe();
}

static uni_error_t my_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    // Accept all gamepads, filter out keyboards
    if (((cod & UNI_BT_COD_MINOR_MASK) & UNI_BT_COD_MINOR_KEYBOARD) == UNI_BT_COD_MINOR_KEYBOARD) {
        logi("Ignoring keyboard\n");
        return UNI_ERROR_IGNORE_DEVICE;
    }
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_device_connected(uni_hid_device_t* d) {
    logi("rc-xbox-controller: device connected: %p\n", d);
}

static void my_platform_on_device_disconnected(uni_hid_device_t* d) {
    logi("rc-xbox-controller: device disconnected: %p\n", d);

    // Safety: stop everything on disconnect
    motor_stop();
    steering_stop();
    led_off();
}

static uni_error_t my_platform_on_device_ready(uni_hid_device_t* d) {
    logi("rc-xbox-controller: device ready: %p\n", d);
    my_platform_instance_t* ins = get_my_platform_instance(d);
    ins->gamepad_seat = GAMEPAD_SEAT_A;

    // Rumble to confirm connection
    if (d->report_parser.play_dual_rumble != NULL) {
        d->report_parser.play_dual_rumble(d, 0, 200, 128, 40);
    }

    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    static uni_controller_t prev = {0};
    static bool prev_y_pressed = false;

    // Skip if no change
    if (memcmp(&prev, ctl, sizeof(*ctl)) == 0) {
        return;
    }
    prev = *ctl;

    if (ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD) {
        return;
    }

    uni_gamepad_t* gp = &ctl->gamepad;

    // --- Drive motors (LT = forward, RT = reverse) ---
    // Bluepad32: brake = LT (0-1023), throttle = RT (0-1023)
    int32_t drive_speed = 0;
    if (gp->brake > 0 && gp->throttle > 0) {
        // Both pressed: safety stop
        drive_speed = 0;
    } else if (gp->brake > 0) {
        drive_speed = gp->brake;     // Forward
    } else if (gp->throttle > 0) {
        drive_speed = -gp->throttle; // Reverse
    }
    motor_set_speed(drive_speed);

    // --- Steering (left thumbstick X-axis) ---
    steering_set_position(gp->axis_x);

    // --- Headlight toggle (Y button, edge-triggered) ---
    bool y_pressed = (gp->buttons & BUTTON_Y) != 0;
    if (y_pressed && !prev_y_pressed) {
        led_toggle();
    }
    prev_y_pressed = y_pressed;
}

static const uni_property_t* my_platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static void my_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    switch (event) {
        case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON:
            logi("rc-xbox-controller: system button pressed\n");
            break;
        case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
            logi("rc-xbox-controller: Bluetooth enabled: %d\n", (bool)(data));
            break;
        default:
            break;
    }
}

//
// Helpers
//

static my_platform_instance_t* get_my_platform_instance(uni_hid_device_t* d) {
    return (my_platform_instance_t*)&d->platform_data[0];
}

//
// Entry Point
//

struct uni_platform* get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "rc-xbox-controller",
        .init = my_platform_init,
        .on_init_complete = my_platform_on_init_complete,
        .on_device_discovered = my_platform_on_device_discovered,
        .on_device_connected = my_platform_on_device_connected,
        .on_device_disconnected = my_platform_on_device_disconnected,
        .on_device_ready = my_platform_on_device_ready,
        .on_oob_event = my_platform_on_oob_event,
        .on_controller_data = my_platform_on_controller_data,
        .get_property = my_platform_get_property,
    };

    return &plat;
}
