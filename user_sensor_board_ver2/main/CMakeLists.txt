idf_component_register(
            SRCS "main.c"
            INCLUDE_DIRS "."
            REQUIRES beacon_scanner common mqtt_common
)
