// SPDX-License-Identifier: MIT
// RC Xbox Controller — Main Entry Point

#include <stdlib.h>

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <btstack_stdio_esp32.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <uni.h>

#include "sdkconfig.h"
#include "webserver.h"
#include "wifi.h"

static const char* TAG = "main";

#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

// Defined in my_platform.c
struct uni_platform* get_my_platform(void);

int app_main(void) {
#ifdef CONFIG_ESP_CONSOLE_UART
#ifndef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
    btstack_stdio_init();
#endif
#endif

    // Initialize NVS — required by Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Connect to Wi-Fi and start OTA web server
    ESP_LOGI(TAG, "Starting Wi-Fi...");
    if (wifi_init_sta() == ESP_OK) {
        ESP_LOGI(TAG, "Starting OTA web server...");
        webserver_start();
    } else {
        ESP_LOGW(TAG, "Wi-Fi failed — OTA disabled, BLE-only mode");
    }

    // Configure BTstack for ESP32 VHCI Controller
    btstack_init();

    // Set our custom platform before init
    uni_platform_set_custom(get_my_platform());

    // Init Bluepad32 — does not return from btstack_run_loop_execute()
    uni_init(0, NULL);
    btstack_run_loop_execute();

    return 0;
}
