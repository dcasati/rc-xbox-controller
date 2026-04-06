// SPDX-License-Identifier: MIT
// RC Xbox Controller — HTTP Server for OTA Upload

#include "webserver.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <string.h>

#include "ota.h"

static const char* TAG = "webserver";

static httpd_handle_t server = NULL;

// HTML upload form — minimal, self-contained
static const char upload_html[] =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>RC Xbox Controller OTA</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:480px;margin:40px auto;padding:0 20px;background:#1a1a2e;color:#e0e0e0;}"
    "h1{color:#0f9;font-size:1.4em;}"
    "form{background:#16213e;padding:20px;border-radius:8px;}"
    "input[type=file]{margin:10px 0;color:#e0e0e0;}"
    "input[type=submit]{background:#0f9;color:#000;border:none;padding:10px 24px;border-radius:4px;cursor:pointer;font-weight:bold;}"
    "input[type=submit]:hover{background:#0da;}"
    "#progress{margin-top:10px;display:none;}"
    "</style></head><body>"
    "<h1>RC Xbox Controller</h1>"
    "<form id='f' method='POST' action='/ota' enctype='multipart/form-data'>"
    "<p>Select firmware binary (.bin):</p>"
    "<input type='file' name='firmware' accept='.bin' required><br><br>"
    "<input type='submit' value='Upload &amp; Flash'>"
    "</form>"
    "<div id='progress'><p>Uploading... do not disconnect power.</p></div>"
    "<script>"
    "document.getElementById('f').onsubmit=function(){"
    "document.getElementById('progress').style.display='block';"
    "};"
    "</script>"
    "</body></html>";

static esp_err_t root_get_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, upload_html, sizeof(upload_html) - 1);
    return ESP_OK;
}

static esp_err_t ota_post_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "OTA upload started (content_len=%d)", req->content_len);

    esp_err_t err = ota_begin();
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    // Read and write in chunks
    char buf[1024];
    int remaining = req->content_len;
    int received;
    bool header_skipped = false;
    size_t firmware_bytes = 0;

    while (remaining > 0) {
        received = httpd_req_recv(req, buf, sizeof(buf) < remaining ? sizeof(buf) : remaining);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "File receive error");
            ota_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }

        // For multipart form data, skip the header on the first chunk
        if (!header_skipped) {
            // Find the end of the multipart header (double CRLF)
            char* body = strstr(buf, "\r\n\r\n");
            if (body) {
                body += 4; // skip past \r\n\r\n
                int header_len = body - buf;
                int body_len = received - header_len;
                if (body_len > 0) {
                    err = ota_write(body, body_len);
                    if (err != ESP_OK) {
                        ota_abort();
                        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
                        return ESP_FAIL;
                    }
                    firmware_bytes += body_len;
                }
                header_skipped = true;
            }
        } else {
            err = ota_write(buf, received);
            if (err != ESP_OK) {
                ota_abort();
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
                return ESP_FAIL;
            }
            firmware_bytes += received;
        }

        remaining -= received;
    }

    // Trim the trailing multipart boundary from the last chunk
    // The OTA validation in ota_end() will catch any corruption

    ESP_LOGI(TAG, "Upload complete (%zu bytes), finalizing...", firmware_bytes);

    // ota_end() will validate, set boot partition, and reboot
    err = ota_end();
    if (err != ESP_OK) {
        // ota_end calls esp_restart on success, so we only get here on failure
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA validation failed");
        return ESP_FAIL;
    }

    // Never reached — device reboots
    return ESP_OK;
}

esp_err_t webserver_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    // Register GET /
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    httpd_register_uri_handler(server, &root);

    // Register POST /ota
    httpd_uri_t ota = {
        .uri = "/ota",
        .method = HTTP_POST,
        .handler = ota_post_handler,
    };
    httpd_register_uri_handler(server, &ota);

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    return ESP_OK;
}

void webserver_stop(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}
