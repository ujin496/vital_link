#include "light_sensor.h"
#include "esp_log.h"

static const char *TAG = "LIGHT_SENSOR";
static adc1_channel_t light_channel;

esp_err_t light_sensor_init(adc1_channel_t channel)
{
    light_channel = channel;

    // ✅ ADC1 레거시 설정: 12비트, 11dB (≈ 3.6V 입력 범위)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(light_channel, ADC_ATTEN_DB_11);

    ESP_LOGI(TAG, "Light sensor initialized (legacy ADC1) on channel %d", channel);
    return ESP_OK;
}

esp_err_t light_sensor_read_raw(int *raw_value)
{
    if (!raw_value) return ESP_ERR_INVALID_ARG;
    *raw_value = adc1_get_raw(light_channel);  // 0 ~ 4095
    return ESP_OK;
}

esp_err_t light_sensor_read_lux(float *lux_value)
{
    if (!lux_value) return ESP_ERR_INVALID_ARG;

    int raw = 0;
    esp_err_t err = light_sensor_read_raw(&raw);
    if (err != ESP_OK) return err;

    float percent = (4095.0f - raw) / 4095.0f; // 0.0 ~ 1.0
    *lux_value = (1000.0f * percent)/10;

    return ESP_OK;
}
