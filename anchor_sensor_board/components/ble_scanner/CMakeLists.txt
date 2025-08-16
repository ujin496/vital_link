idf_component_register(
    SRCS "src/ble_scanner.c"
         "src/esp_ibeacon_api.c"
         "src/sntp_helper.c"
         "src/time_helper.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash bt mqtt_common common
)
