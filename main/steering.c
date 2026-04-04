// SPDX-License-Identifier: MIT
// RC Xbox Controller — Steering Control (TB6612FNG Channel B)

#include "steering.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <stdlib.h>

static const char* TAG = "steering";

// TB6612FNG Channel B — Stepper motor (steering)
#define STEER_BIN1_GPIO  GPIO_NUM_32
#define STEER_BIN2_GPIO  GPIO_NUM_33
#define STEER_PWMB_GPIO  GPIO_NUM_14

// LEDC config — use different channel/timer than drive motor
#define STEER_PWM_CHANNEL   LEDC_CHANNEL_1
#define STEER_PWM_TIMER     LEDC_TIMER_1
#define STEER_PWM_FREQ_HZ   1000
#define STEER_PWM_RESOLUTION LEDC_TIMER_10_BIT  // 0-1023

// Dead zone: ignore thumbstick values near center (~15% of 512)
#define STEER_DEAD_ZONE  80

static int32_t last_logged_pos = 0;

void steering_init(void) {
    // Configure direction GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STEER_BIN1_GPIO) | (1ULL << STEER_BIN2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Configure LEDC PWM for steering speed
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = STEER_PWM_TIMER,
        .duty_resolution = STEER_PWM_RESOLUTION,
        .freq_hz = STEER_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = STEER_PWMB_GPIO,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = STEER_PWM_CHANNEL,
        .timer_sel = STEER_PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel_conf);

    ESP_LOGI(TAG, "Steering initialized (BIN1=%d, BIN2=%d, PWMB=%d)",
             STEER_BIN1_GPIO, STEER_BIN2_GPIO, STEER_PWMB_GPIO);
}

void steering_set_position(int32_t x_axis) {
    uint32_t duty;

    // Apply dead zone
    if (x_axis > -STEER_DEAD_ZONE && x_axis < STEER_DEAD_ZONE) {
        steering_stop();
        return;
    }

    if (x_axis > 0) {
        // Steer right: BIN1=HIGH, BIN2=LOW
        gpio_set_level(STEER_BIN1_GPIO, 1);
        gpio_set_level(STEER_BIN2_GPIO, 0);
        duty = (uint32_t)x_axis;
    } else {
        // Steer left: BIN1=LOW, BIN2=HIGH
        gpio_set_level(STEER_BIN1_GPIO, 0);
        gpio_set_level(STEER_BIN2_GPIO, 1);
        duty = (uint32_t)(-x_axis);
    }

    // Scale from 0-512 range to 0-1023 PWM range
    duty = (duty * 1023) / 512;
    if (duty > 1023) {
        duty = 1023;
    }

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL);

    if (x_axis != last_logged_pos) {
        ESP_LOGI(TAG, "pos=%ld duty=%lu dir=%s", (long)x_axis, (unsigned long)duty, x_axis > 0 ? "RIGHT" : "LEFT");
        last_logged_pos = x_axis;
    }
}

void steering_stop(void) {
    gpio_set_level(STEER_BIN1_GPIO, 0);
    gpio_set_level(STEER_BIN2_GPIO, 0);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL);
}
