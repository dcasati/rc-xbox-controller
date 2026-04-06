// SPDX-License-Identifier: MIT
// RC Xbox Controller — HTTP Server for OTA Upload

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <esp_err.h>

// Start the HTTP server on port 80.
// Serves an upload form at GET / and handles firmware upload at POST /ota.
esp_err_t webserver_start(void);

// Stop the HTTP server.
void webserver_stop(void);

#endif // WEBSERVER_H
