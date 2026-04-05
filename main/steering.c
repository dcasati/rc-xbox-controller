// SPDX-License-Identifier: MIT
// RC Xbox Controller — Steering Control (DC motor + potentiometer feedback)
//
// Uses TB6612FNG Channel B to drive the steering motor.
// Reads a potentiometer on ADC to know the current wheel position.
// Simple proportional control loop: drive motor until pot matches target.

#include "steering.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <stdlib.h>

static const char* TAG = "steering";

// TB6612FNG Channel B — Steering motor
#define STEER_BIN1_GPIO  GPIO_NUM_27
#define STEER_BIN2_GPIO  GPIO_NUM_14
#define STEER_PWMB_GPIO  GPIO_NUM_12

// Potentiometer ADC — GPIO 34 = ADC1_CH6
#define POT_ADC_CHANNEL  ADC_CHANNEL_6
#define POT_ADC_UNIT     ADC_UNIT_1
#define POT_ADC_ATTEN    ADC_ATTEN_DB_12   // 0-3.3V range

// LEDC config
#define STEER_PWM_CHANNEL   LEDC_CHANNEL_1
#define STEER_PWM_TIMER     LEDC_TIMER_1
#define STEER_PWM_FREQ_HZ   1000
#define STEER_PWM_RESOLUTION LEDC_TIMER_10_BIT  // 0-1023

// Dead zone: ignore thumbstick values near center
#define STEER_DEAD_ZONE  80

// Position control
// ADC reads 0-4095 (12-bit). Center ~2048. Adjust after calibration.
#define POT_CENTER    2048
#define POT_MIN       500     // Full left (calibrate)
#define POT_MAX       3500    // Full right (calibrate)
#define POS_TOLERANCE 80      // Stop motor when within this range of target
#define STEER_MIN_PWM 200     // Minimum PWM to overcome static friction
#define STEER_MAX_PWM 800     // Cap max PWM to avoid slamming

static adc_oneshot_unit_handle_t adc_handle = NULL;
static int32_t last_logged_pos = 0;

static int read_pot(void) {
    int val = 0;
    adc_oneshot_read(adc_handle, POT_ADC_CHANNEL, &val);
    return val;
}

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

    // Configure LEDC PWM
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

    // Configure ADC for potentiometer
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = POT_ADC_UNIT,
    };
    adc_oneshot_new_unit(&adc_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = POT_ADC_ATTEN,
    };
    adc_oneshot_config_channel(adc_handle, POT_ADC_CHANNEL, &chan_cfg);

    int pot_val = read_pot();
    ESP_LOGI(TAG, "Steering initialized (BIN1=%d, BIN2=%d, PWMB=%d, POT=GPIO34 val=%d)",
             STEER_BIN1_GPIO, STEER_BIN2_GPIO, STEER_PWMB_GPIO, pot_val);
}

void steering_set_position(int32_t x_axis) {
    // Apply dead zone — target center
    if (x_axis > -STEER_DEAD_ZONE && x_axis < STEER_DEAD_ZONE) {
        x_axis = 0;
    }

    // Map thumbstick (-512..511) to target pot value (POT_MIN..POT_MAX)
    int32_t target = POT_CENTER + (x_axis * (int32_t)(POT_MAX - POT_MIN)) / 1024;
    if (target < POT_MIN) target = POT_MIN;
    if (target > POT_MAX) target = POT_MAX;

    // Read current position
    int current = read_pot();
    int32_t error = target - current;

    // Within tolerance — stop
    if (abs(error) < POS_TOLERANCE) {
        gpio_set_level(STEER_BIN1_GPIO, 0);
        gpio_set_level(STEER_BIN2_GPIO, 0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL, 0);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL);
        return;
    }

    // Direction
    if (error > 0) {
        // Turn right
        gpio_set_level(STEER_BIN1_GPIO, 1);
        gpio_set_level(STEER_BIN2_GPIO, 0);
    } else {
        // Turn left
        gpio_set_level(STEER_BIN1_GPIO, 0);
        gpio_set_level(STEER_BIN2_GPIO, 1);
    }

    // Proportional speed — larger error = faster correction
    uint32_t duty = (abs(error) * STEER_MAX_PWM) / (POT_MAX - POT_CENTER);
    if (duty < STEER_MIN_PWM) duty = STEER_MIN_PWM;
    if (duty > STEER_MAX_PWM) duty = STEER_MAX_PWM;

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL);

    if (x_axis != last_logged_pos) {
        const char* dir = (error > 0) ? "RIGHT" : (error < 0) ? "LEFT" : "CENTER";
        ESP_LOGI(TAG, "target=%ld current=%d error=%ld duty=%lu dir=%s",
                 (long)target, current, (long)error, (unsigned long)duty, dir);
        last_logged_pos = x_axis;
    }
}

void steering_stop(void) {
    gpio_set_level(STEER_BIN1_GPIO, 0);
    gpio_set_level(STEER_BIN2_GPIO, 0);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, STEER_PWM_CHANNEL);
}
