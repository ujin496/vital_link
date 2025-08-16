#ifndef MQTT_CLIENT_WRAPPER_H
#define MQTT_CLIENT_WRAPPER_H

#include "mqtt_client.h"

void mqtt_start(void);  // 초기화만
esp_mqtt_client_handle_t mqtt_get_handle(void);  // 전역 핸들 제공

bool mqtt_is_connected(void);

#endif
