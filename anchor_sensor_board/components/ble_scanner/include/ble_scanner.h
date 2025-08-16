#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BLE Anchor (iBeacon transmitter)
 */
void ble_anchor_init(void);

/**
 * @brief Check if BLE anchor is currently advertising
 * @return true if advertising, false otherwise
 */
bool ble_anchor_is_advertising(void);

/**
 * @brief Restart BLE advertising
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_anchor_restart_advertising(void);

/**
 * @brief Deinitialize BLE anchor
 */
void ble_anchor_deinit(void);

#ifdef __cplusplus
}
#endif