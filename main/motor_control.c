// SPDX-License-Identifier: MIT
// RC Xbox Controller — Motor Control (BTS7960 43A H-Bridge)

#include "motor_control.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>

static const char* TAG = "motor";

// BTS7960 H-Bridge — Drive motors (front + rear in parallel)
// RPWM: forward PWM input  (HIGH = forward, duty sets speed)
// LPWM: reverse PWM input  (HIGH = reverse, duty sets speed)
// R_EN: right/forward half-bridge enable (active HIGH)
// L_EN: left/reverse  half-bridge enable (active HIGH)
#define MOTOR_RPWM_GPIO  GPIO_NUM_32
#define MOTOR_LPWM_GPIO  GPIO_NUM_33
#define MOTOR_R_EN_GPIO  GPIO_NUM_25
#define MOTOR_L_EN_GPIO  GPIO_NUM_26

// LEDC config — two channels share one timer
// NOTE: LEDC_CHANNEL_0 and LEDC_CHANNEL_1 are used here;
//       steering.c uses LEDC_CHANNEL_2 / LEDC_TIMER_1 to avoid conflict.
#define MOTOR_RPWM_CHANNEL   LEDC_CHANNEL_0
#define MOTOR_LPWM_CHANNEL   LEDC_CHANNEL_1
#define MOTOR_PWM_TIMER      LEDC_TIMER_0
#define MOTOR_PWM_FREQ_HZ    20000              // 20 kHz — within BTS7960's 25 kHz limit
#define MOTOR_PWM_RESOLUTION LEDC_TIMER_10_BIT // 0–1023, matches gamepad trigger range

// Dead zone: ignore trigger values below this threshold (~5% of 1023)
#define MOTOR_DEAD_ZONE  50

static int32_t last_logged_speed = 0;

void motor_control_init(void) {
    // Configure enable GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_R_EN_GPIO) | (1ULL << MOTOR_L_EN_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Enable both half-bridges
    gpio_set_level(MOTOR_R_EN_GPIO, 1);
    gpio_set_level(MOTOR_L_EN_GPIO, 1);

    // Configure LEDC timer shared by both PWM channels
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = MOTOR_PWM_TIMER,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .freq_hz = MOTOR_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    // RPWM channel (forward direction)
    ledc_channel_config_t rpwm_conf = {
        .gpio_num = MOTOR_RPWM_GPIO,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = MOTOR_RPWM_CHANNEL,
        .timer_sel = MOTOR_PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&rpwm_conf);

    // LPWM channel (reverse direction)
    ledc_channel_config_t lpwm_conf = {
        .gpio_num = MOTOR_LPWM_GPIO,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = MOTOR_LPWM_CHANNEL,
        .timer_sel = MOTOR_PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&lpwm_conf);

    ESP_LOGI(TAG, "Drive motor initialized (BTS7960: RPWM=%d, LPWM=%d, R_EN=%d, L_EN=%d)",
             MOTOR_RPWM_GPIO, MOTOR_LPWM_GPIO, MOTOR_R_EN_GPIO, MOTOR_L_EN_GPIO);
}

void motor_set_speed(int32_t speed) {
    // Apply dead zone
    if (speed > -MOTOR_DEAD_ZONE && speed < MOTOR_DEAD_ZONE) {
        motor_stop();
        return;
    }

    uint32_t duty = (speed > 0) ? (uint32_t)speed : (uint32_t)(-speed);
    if (duty > 1023) duty = 1023;

    if (speed > 0) {
        // Forward: RPWM = duty, LPWM = 0
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_LPWM_CHANNEL, 0);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, MOTOR_LPWM_CHANNEL);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_RPWM_CHANNEL, duty);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, MOTOR_RPWM_CHANNEL);
    } else {
        // Reverse: LPWM = duty, RPWM = 0
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_RPWM_CHANNEL, 0);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, MOTOR_RPWM_CHANNEL);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_LPWM_CHANNEL, duty);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, MOTOR_LPWM_CHANNEL);
    }

    if (speed != last_logged_speed) {
        ESP_LOGI(TAG, "speed=%ld duty=%lu dir=%s", (long)speed, (unsigned long)duty,
                 speed > 0 ? "FWD" : "REV");
        last_logged_speed = speed;
    }
}

void motor_stop(void) {
    // Coast: both PWM inputs to 0 (motor coasts to a stop)
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_RPWM_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, MOTOR_RPWM_CHANNEL);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_LPWM_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, MOTOR_LPWM_CHANNEL);
    last_logged_speed = 0;
}
