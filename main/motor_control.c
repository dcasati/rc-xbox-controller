// SPDX-License-Identifier: MIT
// RC Xbox Controller — Motor Control (TB6612FNG Channel A)

#include "motor_control.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>

static const char* TAG = "motor";

// TB6612FNG Channel A — Drive motors (front + rear in parallel)
#define MOTOR_AIN1_GPIO  GPIO_NUM_25
#define MOTOR_AIN2_GPIO  GPIO_NUM_33
#define MOTOR_PWMA_GPIO  GPIO_NUM_32
#define MOTOR_STBY_GPIO  GPIO_NUM_26

// LEDC config
#define MOTOR_PWM_CHANNEL  LEDC_CHANNEL_0
#define MOTOR_PWM_TIMER    LEDC_TIMER_0
#define MOTOR_PWM_FREQ_HZ  1000
#define MOTOR_PWM_RESOLUTION LEDC_TIMER_10_BIT  // 0-1023

// Dead zone: ignore trigger values below this threshold (~5% of 1023)
#define MOTOR_DEAD_ZONE  50

static int32_t last_logged_speed = 0;

void motor_control_init(void) {
    // Configure direction GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_AIN1_GPIO) | (1ULL << MOTOR_AIN2_GPIO) | (1ULL << MOTOR_STBY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Enable H-Bridge (STBY HIGH)
    gpio_set_level(MOTOR_STBY_GPIO, 1);

    // Configure LEDC PWM for motor speed
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = MOTOR_PWM_TIMER,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .freq_hz = MOTOR_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = MOTOR_PWMA_GPIO,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = MOTOR_PWM_CHANNEL,
        .timer_sel = MOTOR_PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel_conf);

    ESP_LOGI(TAG, "Drive motor initialized (AIN1=%d, AIN2=%d, PWMA=%d, STBY=%d)",
             MOTOR_AIN1_GPIO, MOTOR_AIN2_GPIO, MOTOR_PWMA_GPIO, MOTOR_STBY_GPIO);
}

void motor_set_speed(int32_t speed) {
    uint32_t duty;

    // Apply dead zone
    if (speed > -MOTOR_DEAD_ZONE && speed < MOTOR_DEAD_ZONE) {
        motor_stop();
        return;
    }

    if (speed > 0) {
        // Forward: AIN1=HIGH, AIN2=LOW
        gpio_set_level(MOTOR_AIN1_GPIO, 1);
        gpio_set_level(MOTOR_AIN2_GPIO, 0);
        duty = (uint32_t)speed;
    } else {
        // Reverse: AIN1=LOW, AIN2=HIGH
        gpio_set_level(MOTOR_AIN1_GPIO, 0);
        gpio_set_level(MOTOR_AIN2_GPIO, 1);
        duty = (uint32_t)(-speed);
    }

    // Clamp to max PWM
    if (duty > 1023) {
        duty = 1023;
    }

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, MOTOR_PWM_CHANNEL);

    if (speed != last_logged_speed) {
        ESP_LOGI(TAG, "speed=%ld duty=%lu dir=%s", (long)speed, (unsigned long)duty, speed > 0 ? "FWD" : "REV");
        last_logged_speed = speed;
    }
}

void motor_stop(void) {
    // Brake: AIN1=LOW, AIN2=LOW, PWM=0
    gpio_set_level(MOTOR_AIN1_GPIO, 0);
    gpio_set_level(MOTOR_AIN2_GPIO, 0);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, MOTOR_PWM_CHANNEL);
}
