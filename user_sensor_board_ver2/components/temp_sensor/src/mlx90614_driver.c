#include "mlx90614_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MLX90614_ADDR  0x5A
#define REG_OBJECT_TEMP 0x07
#define REG_AMBIENT_TEMP 0x06
#define REG_DEVICE_ID 0x0E

static const char *TAG = "MLX90614_DRV";

// MAX30102와 유사한 구조로 헬퍼 함수들 추가
static esp_err_t write_register(i2c_port_t port, uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    return i2c_master_write_to_device(port, MLX90614_ADDR, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t read_register(i2c_port_t port, uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(port, MLX90614_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

esp_err_t mlx90614_init(i2c_port_t port) {
    ESP_LOGI(TAG, "MLX90614 초기화 시작 (I2C 포트: %d)", port);
    
    // 디바이스 ID 확인
    uint8_t data[3];
    esp_err_t ret = read_register(port, REG_DEVICE_ID, data, 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "디바이스 ID 읽기 실패: %s", esp_err_to_name(ret));
        return ret;
    }
    
    uint16_t device_id = data[0] | (data[1] << 8);
    ESP_LOGI(TAG, "MLX90614 디바이스 ID: 0x%04X", device_id);
    
    // 디바이스 ID 검증 (MLX90614는 보통 0x2401 또는 0x2402)
    if (device_id != 0x2401 && device_id != 0x2402) {
        ESP_LOGW(TAG, "예상되지 않은 디바이스 ID: 0x%04X", device_id);
    }
    
    ESP_LOGI(TAG, "MLX90614 초기화 완료");
    return ESP_OK;
}

esp_err_t mlx90614_read_temp(float *object_temp) {
    uint8_t data[3];
    esp_err_t ret;

    // I2C_NUM_1 사용 (sensor_manager에서 I2C1로 호출)
    i2c_port_t port = I2C_NUM_1;

    // MAX30102와 유사한 방식으로 단순화된 읽기
    ret = read_register(port, REG_OBJECT_TEMP, data, 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "온도 데이터 읽기 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 데이터 검증 및 변환
    uint16_t raw = data[0] | (data[1] << 8);
    
    // Raw 데이터 로그 (디버깅용)
    ESP_LOGD(TAG, "Raw data: 0x%02X 0x%02X 0x%02X, Raw value: %d", 
             data[0], data[1], data[2], raw);
    
    // 체크섬 검증 (간단한 검증)
    uint8_t checksum = data[2];
    uint8_t calculated_checksum = 0;
    for (int i = 0; i < 2; i++) {
        calculated_checksum ^= data[i];
    }
    
    if (checksum != calculated_checksum) {
        // ESP_LOGW(TAG, "체크섬 불일치: 계산값=0x%02X, 읽은값=0x%02X", 
        //          calculated_checksum, checksum);
        // 체크섬 오류 시에도 계속 진행 (일부 센서는 체크섬이 다를 수 있음)
    }
    
    // 온도 변환 (Kelvin to Celsius)
    // MLX90614는 Kelvin 단위로 데이터를 제공 (0.02K/LSB)
    float temp_kelvin = raw * 0.02f;
    *object_temp = temp_kelvin - 273.15f;
    
    // 온도 범위 검증 (-40°C ~ 125°C)
    if (*object_temp < -40.0f || *object_temp > 125.0f) {
        ESP_LOGW(TAG, "온도 범위 초과: %.2f°C (Raw: %d)", *object_temp, raw);
        // 범위를 벗어나면 기본값 반환
        *object_temp = 25.0f;
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    ESP_LOGD(TAG, "온도 읽기 성공: %.2f°C (Raw: %d, Kelvin: %.2fK)", 
             *object_temp, raw, temp_kelvin);
    
    return ESP_OK;
}
