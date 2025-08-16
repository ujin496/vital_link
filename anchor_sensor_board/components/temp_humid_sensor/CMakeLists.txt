idf_component_register(
    SRCS "src/temp_humid_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver dht esp_idf_lib_helpers
)
