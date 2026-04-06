// SPDX-License-Identifier: MIT
// RC Xbox Controller — Wi-Fi Station Connection

#ifndef WIFI_H
#define WIFI_H

#include <esp_err.h>

// Initialize Wi-Fi in station mode and connect to configured AP.
// SSID and password are read from Kconfig (menuconfig).
// Blocks until connected or max retries exhausted.
esp_err_t wifi_init_sta(void);

#endif // WIFI_H
