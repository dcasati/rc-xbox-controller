// SPDX-License-Identifier: MIT
// RC Xbox Controller — LED Headlight Control

#include "led.h"

#include <driver/gpio.h>
#include <esp_log.h>

static const char* TAG = "led";

#define LED_GPIO  GPIO_NUM_4

static bool led_state = false;

void led_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0);
    led_state = false;

    ESP_LOGI(TAG, "Headlight LED initialized (GPIO=%d)", LED_GPIO);
}

void led_toggle(void) {
    led_state = !led_state;
    gpio_set_level(LED_GPIO, led_state ? 1 : 0);
    ESP_LOGI(TAG, "Headlight %s", led_state ? "ON" : "OFF");
}

void led_set(bool on) {
    led_state = on;
    gpio_set_level(LED_GPIO, led_state ? 1 : 0);
}

void led_off(void) {
    led_state = false;
    gpio_set_level(LED_GPIO, 0);
}
