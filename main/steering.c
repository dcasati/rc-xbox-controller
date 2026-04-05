// SPDX-License-Identifier: MIT
// RC Xbox Controller — Steering Control (Servo PWM)
//
// Servo expects 50Hz PWM:
//   1.0ms pulse = full left
//   1.5ms pulse = center
//   2.0ms pulse = full right

#include "steering.h"

#include <driver/ledc.h>
#include <esp_log.h>
#include <stdlib.h>

static const char* TAG = "steering";

// Servo signal GPIO (direct from ESP32, no H-Bridge)
#define SERVO_GPIO  GPIO_NUM_13

// LEDC config
#define SERVO_PWM_CHANNEL   LEDC_CHANNEL_1
#define SERVO_PWM_TIMER     LEDC_TIMER_1
#define SERVO_PWM_FREQ_HZ   50
#define SERVO_PWM_RESOLUTION LEDC_TIMER_13_BIT  // 8192 steps per 20ms period

// Servo pulse width in LEDC duty counts (at 13-bit, 50Hz)
// 20ms period = 8192 counts
// 1.0ms = 410 counts (full left)
// 1.5ms = 614 counts (center)
// 2.0ms = 819 counts (full right)
#define SERVO_MIN_DUTY  410
#define SERVO_MID_DUTY  614
#define SERVO_MAX_DUTY  819

// Dead zone: ignore thumbstick values near center (~15% of 512)
#define STEER_DEAD_ZONE  80

static int32_t last_logged_pos = 0;

void steering_init(void) {
    // Configure LEDC PWM for servo
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = SERVO_PWM_TIMER,
        .duty_resolution = SERVO_PWM_RESOLUTION,
        .freq_hz = SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = SERVO_PWM_CHANNEL,
        .timer_sel = SERVO_PWM_TIMER,
        .duty = SERVO_MID_DUTY,
        .hpoint = 0,
    };
    ledc_channel_config(&channel_conf);

    // Start at center
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, SERVO_PWM_CHANNEL, SERVO_MID_DUTY);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, SERVO_PWM_CHANNEL);

    ESP_LOGI(TAG, "Servo steering initialized (GPIO=%d, center=%d)", SERVO_GPIO, SERVO_MID_DUTY);
}

void steering_set_position(int32_t x_axis) {
    uint32_t duty;

    // Apply dead zone — return to center
    if (x_axis > -STEER_DEAD_ZONE && x_axis < STEER_DEAD_ZONE) {
        duty = SERVO_MID_DUTY;
    } else {
        // Map -512..511 → SERVO_MIN_DUTY..SERVO_MAX_DUTY
        // Center at 0 → SERVO_MID_DUTY
        int32_t range = SERVO_MAX_DUTY - SERVO_MIN_DUTY;
        duty = SERVO_MID_DUTY + (x_axis * range) / 1024;

        // Clamp
        if (duty < SERVO_MIN_DUTY) duty = SERVO_MIN_DUTY;
        if (duty > SERVO_MAX_DUTY) duty = SERVO_MAX_DUTY;
    }

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, SERVO_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, SERVO_PWM_CHANNEL);

    if (x_axis != last_logged_pos) {
        const char* dir = (x_axis > STEER_DEAD_ZONE) ? "RIGHT" :
                          (x_axis < -STEER_DEAD_ZONE) ? "LEFT" : "CENTER";
        ESP_LOGI(TAG, "pos=%ld duty=%lu dir=%s", (long)x_axis, (unsigned long)duty, dir);
        last_logged_pos = x_axis;
    }
}

void steering_stop(void) {
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, SERVO_PWM_CHANNEL, SERVO_MID_DUTY);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, SERVO_PWM_CHANNEL);
}
