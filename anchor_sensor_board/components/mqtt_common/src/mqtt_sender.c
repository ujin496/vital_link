#include "mqtt_sender.h"                 // 이 파일의 헤더 (함수 선언 등 포함)
#include "esp_log.h"                     // 로그 출력을 위한 ESP-IDF 헤더
#include "mqtt_client.h"                 // 기본 MQTT 클라이언트 정의
#include "mqtt_client_wrapper.h"         // 커스텀 MQTT 래퍼 함수들 포함 (예: 연결 상태 확인)
#include "esp_timer.h"                   // 타임스탬프(ms) 사용을 위한 타이머 API
#include "esp_ibeacon_api.h"             // vendor_config 구조체 접근을 위한 헤더
#include "sntp_helper.h"

extern esp_mqtt_client_handle_t mqtt_client;  // 외부에서 선언된 MQTT 클라이언트 핸들 사용
extern bool mqtt_is_connected(void);          // MQTT 연결 여부 확인 함수 (래퍼에서 정의)
extern esp_ble_ibeacon_vendor_t vendor_config; // vendor_config 구조체 접근

// 새로운 InfluxDB 형식으로 센서 데이터 전송
void mqtt_send_influx_sensor_data(const influx_sensor_data_t* data) {
    // MQTT 연결이 안 되어 있으면 전송 생략
    if (!mqtt_is_connected() || data == NULL) return;

    // vendor_config에서 major, minor 값 가져오기
    uint16_t major = ENDIAN_CHANGE_U16(vendor_config.major);
    uint16_t minor = ENDIAN_CHANGE_U16(vendor_config.minor);

    char payload[512];
    snprintf(payload, sizeof(payload),
        "{"
        "\"measurement\": \"environment\", "
        "\"tags\": {\"deviceId\": \"%s\"}, "
        "\"fields\": {"
            "\"env_temperature\": %.2f, "
            "\"humidity\": %.2f, "
            "\"tvoc\": %.2f, "
            "\"lux\": %.2f "
            // "\"rs\": %.2f, "
            // "\"ratio\": %.2f"
        "}, "
        "\"location\": {"
            "\"major\": %d, "
            "\"minor\": %d"
            // "\"rssi\": %d"
        "}, "
        "\"time\": %" PRId64
        "}",
        // data->measurement,
        data->device_id,
        data->temperature,
        data->humidity,
        data->tvoc,
        data->lux,
        // data->rs,
        // data->ratio,
        major,
        minor,
        // data->rssi,
        data->timestamp_ms
    );

    // MQTT publish 수행
    esp_mqtt_client_publish(mqtt_client, "sensor/data", payload, 0, 1, 0);

    // 로그 출력: 전송한 payload 내용 표시
    ESP_LOGI("MQTT_SEND", "Published InfluxDB format: %s", payload);
}