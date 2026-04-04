// SPDX-License-Identifier: MIT
// RC Xbox Controller — Main Entry Point

#include <stdlib.h>

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <btstack_stdio_esp32.h>
#include <uni.h>

#include "sdkconfig.h"

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

    // Configure BTstack for ESP32 VHCI Controller
    btstack_init();

    // Set our custom platform before init
    uni_platform_set_custom(get_my_platform());

    // Init Bluepad32 — does not return from btstack_run_loop_execute()
    uni_init(0, NULL);
    btstack_run_loop_execute();

    return 0;
}
