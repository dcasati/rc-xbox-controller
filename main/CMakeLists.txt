set(srcs
    "main.c"
    "my_platform.c"
    "motor_control.c"
    "steering.c"
    "led.c"
    "wifi.c"
    "ota.c"
    "webserver.c"
)

set(requires "bluepad32" "btstack" "driver" "esp_adc"
             "esp_wifi" "esp_http_server" "app_update" "esp_netif"
             "nvs_flash")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
