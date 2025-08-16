idf_build_get_property(idf_path IDF_PATH)

set(nimble_host_dir "${idf_path}/components/bt/host/nimble/nimble/nimble/host/include/host")

idf_component_register(
    SRCS "src/beacon_scanner_task.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "${nimble_host_dir}"
    REQUIRES bt common mqtt_common
)

# REQUIRES nvs_flash mqtt esp_event esp_netif esp_wifi 
