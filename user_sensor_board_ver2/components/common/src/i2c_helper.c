#include "i2c_helper.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "I2C_HELPER";

void i2c_master_init(void) {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    ESP_LOGI(TAG, "I2C 마스터 초기화 시작");
    
    // I2C 초기화 전 충분한 대기 시간
    vTaskDelay(pdMS_TO_TICKS(200));

    // I2C0 초기화 (자이로 센서용)
    i2c_config_t conf0 = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO_0,
        .scl_io_num = I2C_MASTER_SCL_IO_0,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ_0,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM_0, &conf0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C0 파라미터 설정 실패: %s", esp_err_to_name(ret));
        return;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM_0, conf0.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C0 드라이버 설치 실패: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "I2C0 초기화 완료 (SDA: %d, SCL: %d)", I2C_MASTER_SDA_IO_0, I2C_MASTER_SCL_IO_0);

    // I2C1 초기화 (심박/체온 센서용)
    i2c_config_t conf1 = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO_1,
        .scl_io_num = I2C_MASTER_SCL_IO_1,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ_1,
    };

    ret = i2c_param_config(I2C_MASTER_NUM_1, &conf1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C1 파라미터 설정 실패: %s", esp_err_to_name(ret));
        return;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM_1, conf1.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C1 드라이버 설치 실패: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "I2C1 초기화 완료 (SDA: %d, SCL: %d)", I2C_MASTER_SDA_IO_1, I2C_MASTER_SCL_IO_1);
    ESP_LOGI(TAG, "I2C 마스터 초기화 완료");
}

esp_err_t i2c_bus_recover_0(void) {
    ESP_LOGW(TAG, "I2C0 버스 복구 시도");
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << I2C_MASTER_SCL_IO_0),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    for (int i = 0; i < 9; i++) {
        gpio_set_level(I2C_MASTER_SCL_IO_0, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(I2C_MASTER_SCL_IO_0, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    i2c_driver_delete(I2C_MASTER_NUM_0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO_0,
        .scl_io_num = I2C_MASTER_SCL_IO_0,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ_0,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM_0, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C0 파라미터 설정 실패: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM_0, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C0 드라이버 설치 실패: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    
    ESP_LOGW(TAG, "I2C0 버스 복구 완료");
    return ESP_OK;
}

esp_err_t i2c_bus_recover_1(void) {
    ESP_LOGW(TAG, "I2C1 버스 복구 시도");
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << I2C_MASTER_SCL_IO_1),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    for (int i = 0; i < 9; i++) {
        gpio_set_level(I2C_MASTER_SCL_IO_1, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(I2C_MASTER_SCL_IO_1, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    i2c_driver_delete(I2C_MASTER_NUM_1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO_1,
        .scl_io_num = I2C_MASTER_SCL_IO_1,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ_1,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM_1, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C1 파라미터 설정 실패: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM_1, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C1 드라이버 설치 실패: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    
    ESP_LOGW(TAG, "I2C1 버스 복구 완료");
    return ESP_OK;
}
