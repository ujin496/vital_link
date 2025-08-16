// max30102_driver.c

#include "max30102_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAX30102_DRV";

static i2c_port_t current_port = I2C_NUM_0;
static max30102_config_t current_config;

// 기본 설정값 (데이터시트 권장값)
static const max30102_config_t default_config = {
    .led_mode = MAX30102_MODE_SPO2,
    .sample_rate = MAX30102_SAMPLERATE_100,    // 100Hz (SpO2 권장)
    .pulse_width = MAX30102_PULSEWIDTH_411,    // 16bit, 411μs (노이즈에 강함)
    .adc_range = MAX30102_ADCRANGE_4096,       // 4096 nA
    .ir_current = 60,                          // 12mA (60 * 0.2mA)
    .red_current = 60,                         // 12mA (60 * 0.2mA)
    .sample_averaging = 4,                     // 4 샘플 평균
    .fifo_rollover = true                      // FIFO 롤오버 활성화
};

static esp_err_t write_register(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    esp_err_t ret = i2c_master_write_to_device(current_port, MAX30102_I2C_ADDR, data, 2, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "레지스터 쓰기 실패 - Reg: 0x%02X, Val: 0x%02X, Err: %s", 
                 reg, val, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t read_register(uint8_t reg, uint8_t *data, size_t len) {
    esp_err_t ret = i2c_master_write_read_device(current_port, MAX30102_I2C_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "레지스터 읽기 실패 - Reg: 0x%02X, Err: %s", reg, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t max30102_init(i2c_port_t port) {
    return max30102_init_advanced(port, &default_config);
}

esp_err_t max30102_init_advanced(i2c_port_t port, const max30102_config_t *config) {
    ESP_LOGI(TAG, "MAX30102 초기화 시작 (포트: %d)", port);
    current_port = port;
    current_config = *config;
    
    // Part ID 확인
    uint8_t part_id;
    esp_err_t ret = read_register(MAX30102_REG_PART_ID, &part_id, 1);
    if (ret != ESP_OK || part_id != 0x15) {
        ESP_LOGE(TAG, "MAX30102 센서를 찾을 수 없습니다 (Part ID: 0x%02X)", part_id);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "MAX30102 센서 감지됨 (Part ID: 0x%02X)", part_id);
    
    // 소프트웨어 리셋
    ret = write_register(MAX30102_REG_MODE_CONFIG, 0x40);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // FIFO 설정
    uint8_t fifo_config = 0x00;
    fifo_config |= (config->sample_averaging << 5);  // SMP_AVE[2:0]
    if (config->fifo_rollover) {
        fifo_config |= 0x10;  // FIFO_ROLLOVER_EN
    }
    fifo_config |= 0x0F;  // FIFO_A_FULL = 15 (32-17=15 empty spaces)
    ret = write_register(MAX30102_REG_FIFO_CONFIG, fifo_config);
    if (ret != ESP_OK) return ret;
    
    // 모드 설정
    ret = write_register(MAX30102_REG_MODE_CONFIG, config->led_mode);
    if (ret != ESP_OK) return ret;
    
    // SpO2 설정 (샘플레이트, ADC 범위, 펄스 폭)
    uint8_t spo2_config = 0x00;
    spo2_config |= (config->adc_range << 5);     // SPO2_ADC_RGE[1:0]
    spo2_config |= (config->sample_rate << 2);   // SPO2_SR[2:0]
    spo2_config |= config->pulse_width;          // LED_PW[1:0]
    ret = write_register(MAX30102_REG_SPO2_CONFIG, spo2_config);
    if (ret != ESP_OK) return ret;
    
    // LED 전류 설정
    ret = max30102_set_led_current(config->ir_current, config->red_current);
    if (ret != ESP_OK) return ret;
    
    // Multi-LED 모드 설정 (SpO2 모드용)
    if (config->led_mode == MAX30102_MODE_SPO2 || config->led_mode == MAX30102_MODE_MULTI_LED) {
        ret = write_register(MAX30102_REG_MULTI_LED_CTRL1, 0x21); // SLOT1=RED, SLOT2=IR
        if (ret != ESP_OK) return ret;
        ret = write_register(MAX30102_REG_MULTI_LED_CTRL2, 0x00); // SLOT3,4 비활성화
        if (ret != ESP_OK) return ret;
    }
    
    // FIFO 포인터 초기화
    ret = max30102_clear_fifo();
    if (ret != ESP_OK) return ret;
    
    ESP_LOGI(TAG, "MAX30102 초기화 완료 - 모드: %d, 샘플레이트: %dHz, LED 전류: IR=%dmA, RED=%dmA", 
             config->led_mode, 
             (50 << config->sample_rate),
             config->ir_current * 200 / 1000,  // mA 변환
             config->red_current * 200 / 1000);
    
    return ESP_OK;
}

esp_err_t max30102_read_fifo(uint32_t *red, uint32_t *ir) {
    uint8_t data[6];
    esp_err_t ret = read_register(MAX30102_REG_FIFO_DATA, data, 6);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 18비트 데이터 추출 (상위 6비트는 무시)
    *red = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    *red &= 0x3FFFF;  // 18비트 마스크
    
    *ir = ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];
    *ir &= 0x3FFFF;   // 18비트 마스크
    
    return ESP_OK;
}

uint8_t max30102_read_fifo_multi(max30102_fifo_data_t *fifo_data, uint8_t max_samples) {
    uint8_t samples_available = max30102_get_fifo_samples_available();
    uint8_t samples_to_read = (samples_available > max_samples) ? max_samples : samples_available;
    
    fifo_data->samples_available = samples_to_read;
    fifo_data->fifo_overflow = false;
    
    // FIFO 오버플로우 확인
    uint8_t overflow_counter;
    if (read_register(MAX30102_REG_FIFO_OVF_CNT, &overflow_counter, 1) == ESP_OK) {
        if (overflow_counter > 0) {
            fifo_data->fifo_overflow = true;
            ESP_LOGW(TAG, "FIFO 오버플로우 감지: %d", overflow_counter);
        }
    }
    
    // 최신 샘플만 읽기 (가장 최근 1개)
    if (samples_to_read > 0) {
        max30102_read_fifo(&fifo_data->red, &fifo_data->ir);
        return 1;
    }
    
    return 0;
}

esp_err_t max30102_set_led_current(uint8_t ir_current, uint8_t red_current) {
    esp_err_t ret1 = write_register(MAX30102_REG_LED1_PA, ir_current);
    esp_err_t ret2 = write_register(MAX30102_REG_LED2_PA, red_current);
    
    if (ret1 == ESP_OK && ret2 == ESP_OK) {
        current_config.ir_current = ir_current;
        current_config.red_current = red_current;
        ESP_LOGD(TAG, "LED 전류 설정: IR=%dmA, RED=%dmA", 
                ir_current * 200 / 1000, red_current * 200 / 1000);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t max30102_clear_fifo(void) {
    esp_err_t ret1 = write_register(MAX30102_REG_FIFO_WR_PTR, 0x00);
    esp_err_t ret2 = write_register(MAX30102_REG_FIFO_RD_PTR, 0x00);
    esp_err_t ret3 = write_register(MAX30102_REG_FIFO_OVF_CNT, 0x00);
    
    return (ret1 == ESP_OK && ret2 == ESP_OK && ret3 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

uint8_t max30102_get_fifo_samples_available(void) {
    uint8_t wr_ptr, rd_ptr;
    
    if (read_register(MAX30102_REG_FIFO_WR_PTR, &wr_ptr, 1) != ESP_OK ||
        read_register(MAX30102_REG_FIFO_RD_PTR, &rd_ptr, 1) != ESP_OK) {
        return 0;
    }
    
    // FIFO는 32개 슬롯을 가지며 순환 구조
    uint8_t samples = 0;
    if (wr_ptr >= rd_ptr) {
        samples = wr_ptr - rd_ptr;
    } else {
        samples = (32 - rd_ptr) + wr_ptr;
    }
    
    return samples;
}

esp_err_t max30102_start_temperature_measurement(void) {
    return write_register(MAX30102_REG_TEMP_CONFIG, 0x01);
}

esp_err_t max30102_read_temperature(float *temperature) {
    uint8_t temp_int, temp_frac;
    esp_err_t ret1 = read_register(MAX30102_REG_TEMP_INT, &temp_int, 1);
    esp_err_t ret2 = read_register(MAX30102_REG_TEMP_FRAC, &temp_frac, 1);
    
    if (ret1 == ESP_OK && ret2 == ESP_OK) {
        *temperature = (float)(int8_t)temp_int + ((float)temp_frac * 0.0625f);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t max30102_check_status(void) {
    uint8_t mode_config;
    esp_err_t ret = read_register(MAX30102_REG_MODE_CONFIG, &mode_config, 1);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "센서 상태 정상 - 모드: 0x%02X", mode_config);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "센서 상태 확인 실패");
        return ESP_FAIL;
    }
}
