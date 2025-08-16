#include "mpu6050_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define MPU6050_ADDR           0x68
#define MPU6050_PWR_MGMT_1     0x6B
#define MPU6050_ACCEL_CONFIG   0x1C  // 가속도 설정 레지스터
#define MPU6050_GYRO_CONFIG    0x1B  // 자이로 설정 레지스터
#define MPU6050_ACCEL_XOUT_H   0x3B
#define I2C_TIMEOUT_MS         100

static const char* TAG = "MPU6050";

esp_err_t mpu6050_init(i2c_port_t port) {
    uint8_t data[2];
    esp_err_t err;

    ESP_LOGI(TAG, "MPU6050 초기화 시작 (I2C 포트: %d, 주소: 0x%02X)", port, MPU6050_ADDR);

    // 센서 초기화 전 대기
    vTaskDelay(pdMS_TO_TICKS(100));

    // 먼저 센서가 응답하는지 확인
    uint8_t who_am_i_reg = 0x75;  // WHO_AM_I 레지스터
    uint8_t who_am_i_value;
    
    err = i2c_master_write_read_device(port, MPU6050_ADDR,
                                      &who_am_i_reg, 1,
                                      &who_am_i_value, 1,
                                      pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 WHO_AM_I 읽기 실패: %s", esp_err_to_name(err));
        return err;
    }
    
    if (who_am_i_value != 0x68) {
        ESP_LOGE(TAG, "MPU6050 WHO_AM_I 값 오류: 0x%02X (예상: 0x68)", who_am_i_value);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "MPU6050 WHO_AM_I 확인됨: 0x%02X", who_am_i_value);

    // Wake up sensor (clear sleep bit)
    data[0] = MPU6050_PWR_MGMT_1;
    data[1] = 0x00;
    err = i2c_master_write_to_device(port, MPU6050_ADDR, data, 2, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 웨이크업 실패: %s", esp_err_to_name(err));
        return err;
    }

    // 가속도 범위 설정: ±2g (AFS_SEL = 0)
    // 논문에서는 ±16g를 사용했지만, ±2g도 낙상 감지에 충분함
    data[0] = MPU6050_ACCEL_CONFIG;
    data[1] = 0x00;  // ±2g (16384 LSB/g)
    err = i2c_master_write_to_device(port, MPU6050_ADDR, data, 2, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 가속도 설정 실패: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "가속도 범위: ±2g 설정");

    // 자이로 범위 설정: ±2000 dps (FS_SEL = 3)
    data[0] = MPU6050_GYRO_CONFIG;
    data[1] = 0x18;  // ±2000 dps (16.4 LSB/dps)
    err = i2c_master_write_to_device(port, MPU6050_ADDR, data, 2, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 자이로 설정 실패: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "자이로 범위: ±2000 dps 설정");

    // 초기화 후 대기
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "MPU6050 초기화 완료 (논문 기반 낙상 감지 준비)");
    return ESP_OK;
}

esp_err_t mpu6050_read_data(i2c_port_t port, mpu6050_data_t *out_data) {
    uint8_t reg = MPU6050_ACCEL_XOUT_H;
    uint8_t buffer[14];

    esp_err_t err = i2c_master_write_read_device(port, MPU6050_ADDR,
                                                 &reg, 1,
                                                 buffer, sizeof(buffer),
                                                 pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MPU6050 data: %s", esp_err_to_name(err));
        return err;
    }

    out_data->ax = (int16_t)(buffer[0] << 8 | buffer[1]);
    out_data->ay = (int16_t)(buffer[2] << 8 | buffer[3]);
    out_data->az = (int16_t)(buffer[4] << 8 | buffer[5]);
    out_data->gx = (int16_t)(buffer[8] << 8 | buffer[9]);
    out_data->gy = (int16_t)(buffer[10] << 8 | buffer[11]);
    out_data->gz = (int16_t)(buffer[12] << 8 | buffer[13]);

    return ESP_OK;
}
