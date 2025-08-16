idf_component_register(
    SRCS "src/mqtt_client_wrapper.c"
         "src/mqtt_sender.c"
         "src/send_task.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt common
)