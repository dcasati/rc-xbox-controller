// SPDX-License-Identifier: MIT
// RC Xbox Controller — Steering Control (TB6612FNG Channel B + potentiometer feedback)
//
// Drives a DC steering motor via TB6612FNG Channel B with closed-loop position
// control using a potentiometer wiper (ADC1_CH6 / GPIO 34).
//
// TB6612FNG Channel B wiring:
//   BIN1 → GPIO 27   (direction A)
//   BIN2 → GPIO 14   (direction B)
//   PWMB → GPIO 12   (PWM speed, LEDC CH2 / TIM1)
//   STBY → GPIO 26   (shared with drive motor; held HIGH by motor_control_init)
//   BO1  → Motor +
//   BO2  → Motor −
//
// Potentiometer wiring:
//   White  → ESP32 3.3 V
//   Yellow → GPIO 34   (ADC1_CH6, wiper)
//   Brown  → GND
//
// Pot is inverted: higher ADC value = left, lower = right.
// Calibrated values from hardware: CENTER=2160, LEFT=2475, RIGHT=1200.
// Left thumbstick X-axis (-512 … +511) maps to a target ADC count.
// A proportional controller drives the motor toward that target and coasts
// when the stick is in the dead zone or error is within tolerance.

#include "steering.h"

#include <stdlib.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>

static const char *TAG = "steering";

// ---- TB6612FNG Channel B GPIOs ---------------------------------------------
#define STEER_BIN1_GPIO   GPIO_NUM_27
#define STEER_BIN2_GPIO   GPIO_NUM_14
#define STEER_PWMB_GPIO   GPIO_NUM_12

// ---- Potentiometer ADC -----------------------------------------------------
#define STEER_POT_ADC_UNIT    ADC_UNIT_1
#define STEER_POT_ADC_CHANNEL ADC_CHANNEL_6   // GPIO 34 = ADC1_CH6

// ---- LEDC (PWMB) -----------------------------------------------------------
// Channel 2 / Timer 1 — clear of drive motor (CH0, CH1 / TIM0)
#define STEER_LEDC_CHANNEL    LEDC_CHANNEL_2
#define STEER_LEDC_TIMER      LEDC_TIMER_1
#define STEER_PWM_FREQ_HZ     20000              // 20 kHz — inaudible, within TB6612FNG spec
#define STEER_PWM_RESOLUTION  LEDC_TIMER_10_BIT  // 0–1023

// ---- Potentiometer calibration (hardware-verified) -------------------------
// Pot is inverted: higher value = left, lower value = right.
#define POT_CENTER   2160
#define POT_LEFT     2475   // full-left hard stop
#define POT_RIGHT    1200   // full-right hard stop

// ---- P-controller tuning ---------------------------------------------------
#define STEER_MIN_DUTY   150   // minimum duty to overcome static friction
#define STEER_MAX_DUTY   1023   // ~88% of 1023 — fast slew, leaves headroom

// ---- Dead zones ------------------------------------------------------------
#define STEER_POT_TOLERANCE  80   // ADC counts — coast inside this band
#define STEER_AXIS_DEAD_ZONE 80   // thumbstick units — coast, don't drive to center

// ---- ADC averaging ---------------------------------------------------------
#define POT_BURST_SAMPLES   32
#define POT_HISTORY_SIZE     4

// ---- State -----------------------------------------------------------------
static adc_oneshot_unit_handle_t s_adc_handle      = NULL;
static int32_t                   last_logged_axis   = 0;
static int                       pot_history[POT_HISTORY_SIZE];
static int                       pot_history_idx    = 0;
static bool                      pot_history_filled = false;

// ---- Private helpers -------------------------------------------------------

static void steer_coast(void) {
    gpio_set_level(STEER_BIN1_GPIO, 0);
    gpio_set_level(STEER_BIN2_GPIO, 0);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, STEER_LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, STEER_LEDC_CHANNEL);
}

static int read_pot_raw(void) {
    int32_t sum = 0;
    for (int i = 0; i < POT_BURST_SAMPLES; i++) {
        int val = 0;
        adc_oneshot_read(s_adc_handle, STEER_POT_ADC_CHANNEL, &val);
        sum += val;
    }
    return (int)(sum / POT_BURST_SAMPLES);
}

static int32_t read_pot(void) {
    int raw = read_pot_raw();
    pot_history[pot_history_idx] = raw;
    pot_history_idx = (pot_history_idx + 1) % POT_HISTORY_SIZE;
    if (pot_history_idx == 0) pot_history_filled = true;

    int count = pot_history_filled ? POT_HISTORY_SIZE : pot_history_idx;
    if (count == 0) return (int32_t)raw;

    int32_t avg = 0;
    for (int i = 0; i < count; i++) avg += pot_history[i];
    return avg / count;
}

// ---- Public API ------------------------------------------------------------

void steering_init(void) {
    // BIN1, BIN2 as outputs (coast: both low)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STEER_BIN1_GPIO) | (1ULL << STEER_BIN2_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(STEER_BIN1_GPIO, 0);
    gpio_set_level(STEER_BIN2_GPIO, 0);

    // LEDC timer for PWMB
    ledc_timer_config_t timer_conf = {
        .speed_mode      = LEDC_HIGH_SPEED_MODE,
        .timer_num       = STEER_LEDC_TIMER,
        .duty_resolution = STEER_PWM_RESOLUTION,
        .freq_hz         = STEER_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    // LEDC channel for PWMB
    ledc_channel_config_t ch_conf = {
        .gpio_num   = STEER_PWMB_GPIO,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel    = STEER_LEDC_CHANNEL,
        .timer_sel  = STEER_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_conf);

    // ADC1 for potentiometer wiper
    adc_oneshot_unit_init_cfg_t adc_init = {
        .unit_id  = STEER_POT_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&adc_init, &s_adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,   // 0–3.3 V full-scale
        .bitwidth = ADC_BITWIDTH_12,   // 0–4095
    };
    adc_oneshot_config_channel(s_adc_handle, STEER_POT_ADC_CHANNEL, &chan_cfg);

    // Prime the running average buffer
    for (int i = 0; i < POT_HISTORY_SIZE; i++) pot_history[i] = read_pot_raw();
    pot_history_filled = true;

    int pot_val = read_pot();
    ESP_LOGI(TAG, "Steering init: BIN1=GPIO%d BIN2=GPIO%d PWMB=GPIO%d POT=ADC1_CH%d val=%d",
             STEER_BIN1_GPIO, STEER_BIN2_GPIO, STEER_PWMB_GPIO, STEER_POT_ADC_CHANNEL, pot_val);
}

void steering_set_position(int32_t x_axis) {
    // Dead zone — coast, don't drive to center
    if (x_axis > -STEER_AXIS_DEAD_ZONE && x_axis < STEER_AXIS_DEAD_ZONE) {
        steer_coast();
        return;
    }

    // Map thumbstick to target ADC count.
    // Pot is inverted: left stick (negative x) → higher pot value (POT_LEFT)
    //                  right stick (positive x) → lower pot value (POT_RIGHT)
    int32_t target;
    if (x_axis < 0) {
        // Left: map -512..-(DEAD_ZONE) → POT_LEFT..POT_CENTER
        target = POT_CENTER + (-x_axis * (int32_t)(POT_LEFT - POT_CENTER)) / 512;
    } else {
        // Right: map +(DEAD_ZONE)..+511 → POT_CENTER..POT_RIGHT
        target = POT_CENTER - (x_axis * (int32_t)(POT_CENTER - POT_RIGHT)) / 512;
    }
    if (target > POT_LEFT)  target = POT_LEFT;
    if (target < POT_RIGHT) target = POT_RIGHT;

    // Read current position and compute error
    int32_t current = read_pot();
    int32_t error   = target - current;

    if (x_axis != last_logged_axis) {
        const char *dir_str = (error > 0) ? "LEFT" : (error < 0) ? "RIGHT" : "CENTER";
        ESP_LOGI(TAG, "axis=%ld target=%ld current=%ld error=%ld dir=%s",
                 (long)x_axis, (long)target, (long)current, (long)error, dir_str);
        last_logged_axis = x_axis;
    }

    // Within tolerance → coast
    if (abs((int)error) < STEER_POT_TOLERANCE) {
        steer_coast();
        return;
    }

    // Direction: error>0 means pot needs to increase (move left)
    if (error > 0) {
        gpio_set_level(STEER_BIN1_GPIO, 1);
        gpio_set_level(STEER_BIN2_GPIO, 0);
    } else {
        gpio_set_level(STEER_BIN1_GPIO, 0);
        gpio_set_level(STEER_BIN2_GPIO, 1);
    }

    // P-controller: duty proportional to |error|
    int32_t duty = (abs((int)error) * STEER_MAX_DUTY) / (POT_LEFT - POT_RIGHT);
    if (duty < STEER_MIN_DUTY) duty = STEER_MIN_DUTY;
    if (duty > STEER_MAX_DUTY) duty = STEER_MAX_DUTY;

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, STEER_LEDC_CHANNEL, (uint32_t)duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, STEER_LEDC_CHANNEL);
}

void steering_stop(void) {
    steer_coast();
    ESP_LOGI(TAG, "Steering stopped (coast)");
}
