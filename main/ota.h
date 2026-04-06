// SPDX-License-Identifier: MIT
// RC Xbox Controller — OTA Update Handler

#ifndef OTA_H
#define OTA_H

#include <esp_err.h>
#include <stddef.h>

// Begin an OTA update session. Must be called before writing chunks.
esp_err_t ota_begin(void);

// Write a chunk of firmware data. Call repeatedly as data arrives.
esp_err_t ota_write(const char* data, size_t len);

// Finalize OTA, validate image, set boot partition, and reboot.
esp_err_t ota_end(void);

// Abort an in-progress OTA update.
void ota_abort(void);

#endif // OTA_H
