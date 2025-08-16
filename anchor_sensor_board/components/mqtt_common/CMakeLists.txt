idf_component_register(
    SRCS "src/mqtt_client_wrapper.c"
         "src/mqtt_sender.c"
         "src/send_task.c"
    INCLUDE_DIRS "include"
    REQUIRES common mqtt tvoc_sensor temp_humid_sensor ble_scanner light_sensor
)