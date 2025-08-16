// mqtt_client_wrapper.c

#include "mqtt_client_wrapper.h"
#include "esp_log.h"
#include "sntp_helper.h"

// mqtt_client 전역 변수로 관리
esp_mqtt_client_handle_t mqtt_client = NULL;

static const char *TAG = "MQTT";

static bool mqtt_connected = false;

bool mqtt_is_connected(void) {
    return mqtt_connected;
}

// MQTT 이벤트 핸들러
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    if (event_id == MQTT_EVENT_CONNECTED) {
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT connected");
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected");
    }
}

// MQTT 설정, 초기화 및 시작 함수
void mqtt_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://i13a107.p.ssafy.io:8883",
        .credentials.username = "a107",
        .credentials.authentication.password = "123456789",
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);  // 전역으로 관리되는 mqtt_client 변수 초기화
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);     // mqtt_client 시작
}

esp_mqtt_client_handle_t mqtt_get_handle(void) {
    return mqtt_client;
}