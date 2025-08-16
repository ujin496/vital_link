idf_component_register(
    SRCS "src/wifi_connect.c"
         "src/sensor_data.c"
         "src/i2c_helper.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash mqtt esp_event esp_netif esp_wifi driver
)