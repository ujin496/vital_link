idf_component_register(
            SRCS "main.c"
            INCLUDE_DIRS "."
            REQUIRES common mqtt_common tvoc_sensor temp_humid_sensor ble_scanner light_sensor esp_adc
)
