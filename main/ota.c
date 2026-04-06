// SPDX-License-Identifier: MIT
// RC Xbox Controller — OTA Update Handler

#include "ota.h"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

static const char* TAG = "ota";

static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t* update_partition = NULL;
static size_t total_written = 0;

esp_err_t ota_begin(void) {
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Writing to partition: %s (offset 0x%lx)",
             update_partition->label, (unsigned long)update_partition->address);

    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    total_written = 0;
    ESP_LOGI(TAG, "OTA update started");
    return ESP_OK;
}

esp_err_t ota_write(const char* data, size_t len) {
    esp_err_t err = esp_ota_write(ota_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        return err;
    }

    total_written += len;
    return ESP_OK;
}

esp_err_t ota_end(void) {
    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "OTA update complete (%zu bytes written)", total_written);

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Boot partition set to %s — rebooting...", update_partition->label);
    esp_restart();

    // Never reached
    return ESP_OK;
}

void ota_abort(void) {
    if (ota_handle != 0) {
        esp_ota_abort(ota_handle);
        ota_handle = 0;
        total_written = 0;
        ESP_LOGW(TAG, "OTA update aborted");
    }
}
