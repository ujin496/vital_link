#include "sensor_manager.h"
#include "i2c_helper.h"
#include "mpu6050_driver.h"
#include "max30102_driver.h"
#include "heart_rate_calculator.h"
#include "mlx90614_driver.h"
#include "sensor_data.h"
#include "mpu6050_step_fall.h"  // 추가

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

static const char *TAG = "SENSOR_MANAGER";

// 태스크 핸들
static TaskHandle_t sensor_manager_task_handle = NULL;

// I2C 동기화를 위한 전역 뮤텍스 (각 I2C 포트별로 분리)
static SemaphoreHandle_t i2c0_mutex = NULL;  // 자이로 센서용
static SemaphoreHandle_t i2c1_mutex = NULL;  // 심박/체온 센서용

// 태스크 종료 플래그
static bool task_running = false;

// 센서 데이터 구조체들
static mpu6050_data_t mpu6050_data;
static uint32_t max30102_red, max30102_ir;
static float mlx90614_temp;

// 센서 초기화 상태 플래그 추가
static bool mpu6050_initialized = false;
static bool max30102_initialized = false;
static bool mlx90614_initialized = false;

// 걸음 수 및 낙상 감지 컨텍스트 추가
static step_fall_ctx_t step_fall_ctx;
static bool step_fall_initialized = false;

// 기존 전역 변수 방식 유지 (걸음 수 누적용)
static int step_count = 0;

/**
 * @brief MPU6050 센서 읽기 함수 (I2C0 사용) - 고급 알고리즘 적용
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
static esp_err_t read_mpu6050(void) {
    esp_err_t ret = mpu6050_read_data(I2C_MASTER_NUM_0, &mpu6050_data);
    if (ret == ESP_OK) {
        // 걸음 수 및 낙상 감지 초기화 (한 번만)
        if (!step_fall_initialized) {
            step_fall_init(&step_fall_ctx, 100.0f); // 100Hz 샘플링
            step_fall_initialized = true;
            ESP_LOGI(TAG, "걸음 수 및 낙상 감지 알고리즘 초기화 완료");
        }

        // 현재 시간을 ms 단위로 변환
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        
        // 고급 걸음 수 감지 알고리즘 실행 (누적 방식)
        if (step_fall_detect_step(&step_fall_ctx,
                                 mpu6050_data.ax, mpu6050_data.ay, mpu6050_data.az,
                                 mpu6050_data.gx, mpu6050_data.gy, mpu6050_data.gz,
                                 now_ms)) {
            step_count++; // 기존 방식대로 누적
            sensor_data_set_steps(step_count);
        }
        
        // 고급 낙상 감지 알고리즘 실행 (논문 기반)
        static bool fall_detected_flag = false;  // 낙상 감지 플래그
        static uint32_t fall_reset_time = 0;     // 리셋 시간
        
        fall_result_t fall_result = step_fall_detect_fall(&step_fall_ctx,
                                                         mpu6050_data.ax, mpu6050_data.ay, mpu6050_data.az,
                                                         mpu6050_data.gx, mpu6050_data.gy, mpu6050_data.gz,
                                                         now_ms);
        
        if (fall_result.fall_detected) {
            if (!fall_detected_flag) {  // 처음 감지된 경우에만
                // 방향 문자열 변환
                const char* direction_names[] = {
                    "없음", "앞", "뒤", "좌", "우", 
                    "앞-좌", "앞-우", "뒤-좌", "뒤-우"
                };
                
                ESP_LOGW(TAG, "논문 기반 낙상 감지됨!");
                ESP_LOGW(TAG, "가속도: X=%.3fg, Y=%.3fg", fall_result.ax_g, fall_result.ay_g);
                ESP_LOGW(TAG, "각도: Roll=%.1f°, Pitch=%.1f°", fall_result.roll_deg, fall_result.pitch_deg);
                ESP_LOGW(TAG, "낙상 방향: %s (각도: %.1f°)", direction_names[fall_result.direction], fall_result.fall_angle_deg);
                ESP_LOGW(TAG, "fallDetected=1 설정");
                
                sensor_data_set_fall_detected(1);
                fall_detected_flag = true;
                fall_reset_time = now_ms + 3000;  // 3초 후 리셋 예약
            }
        }
        
        // 자동 리셋 시간이 되면 0으로 설정
        if (fall_detected_flag && now_ms >= fall_reset_time) {
            ESP_LOGD(TAG, "fallDetected 자동 리셋 - fallDetected=0 설정");
            sensor_data_set_fall_detected(0);
            fall_detected_flag = false;  // 리셋 완료
            fall_reset_time = 0;
        }
    }
    return ret;
}

/**
 * @brief MAX30102 센서 읽기 함수 (I2C1 사용)
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
static esp_err_t read_max30102(void) {
    esp_err_t ret = max30102_read_fifo(&max30102_red, &max30102_ir);
    
    if (ret == ESP_OK) {
        // 심박수 및 SpO2 계산
        heart_rate_data_t heart_data = calculate_heart_rate_and_spo2(max30102_red, max30102_ir);
        
        if (heart_data.valid_data) {
            sensor_data_set_heart_rate(heart_data.heart_rate);
            sensor_data_set_spo2(heart_data.spo2);
        }
    }
    
    return ret;
}

/**
 * @brief MLX90614 센서 읽기 함수 (I2C1 사용)
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
static esp_err_t read_mlx90614(void) {
    esp_err_t ret = mlx90614_read_temp(&mlx90614_temp);
    if (ret == ESP_OK) {
        sensor_data_set_temperature(mlx90614_temp);
    }
    return ret;
}

/**
 * @brief 센서 읽기 함수 (재시도 로직 포함, I2C 포트별 분리)
 * @param sensor_read_func 센서 읽기 함수 포인터
 * @param sensor_name 센서 이름 (로그용)
 * @param max_retries 최대 재시도 횟수
 * @param use_i2c0 true면 I2C0, false면 I2C1 사용
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
static esp_err_t read_sensor_with_retry(esp_err_t (*sensor_read_func)(void), 
                                       const char *sensor_name, 
                                       int max_retries,
                                       bool use_i2c0) {
    esp_err_t ret = ESP_FAIL;
    int retry_count = 0;
    SemaphoreHandle_t mutex = use_i2c0 ? i2c0_mutex : i2c1_mutex;
    
    while (retry_count < max_retries) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
            ESP_LOGE(TAG, "%s: I2C 뮤텍스 획득 실패", sensor_name);
            return ESP_FAIL;
        }
        
        // I2C 버스 복구 (재시도 시에만)
        if (retry_count > 0) {
            if (use_i2c0) {
                i2c_bus_recover_0();
            } else {
                i2c_bus_recover_1();
            }
        }
        
        ret = sensor_read_func();
        
        xSemaphoreGive(mutex);
        
        if (ret == ESP_OK) {
            if (retry_count > 0) {
                ESP_LOGW(TAG, "%s: %d번째 재시도 후 성공", sensor_name, retry_count);
            }
            return ESP_OK;
        }
        
        retry_count++;
        ESP_LOGW(TAG, "%s: 읽기 실패 (%d/%d): %s", 
                sensor_name, retry_count, max_retries, esp_err_to_name(ret));
        
        // 재시도 전 대기
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    ESP_LOGE(TAG, "%s: 최대 재시도 횟수 초과", sensor_name);
    return ESP_FAIL;
}

/**
 * @brief 센서 매니저 태스크
 * @param pvParameters 태스크 파라미터 (사용하지 않음)
 */
static void sensor_manager_task(void *pvParameters) {
    ESP_LOGI(TAG, "센서 매니저 태스크 시작");
    
    TickType_t last_mpu6050_time = 0;
    TickType_t last_max30102_time = 0;
    TickType_t last_mlx90614_time = 0;
    
    const TickType_t mpu6050_interval = pdMS_TO_TICKS(10);   // 10ms (100Hz)
    const TickType_t max30102_interval = pdMS_TO_TICKS(20);  // 20ms (50Hz)
    const TickType_t mlx90614_interval = pdMS_TO_TICKS(1000); // 1000ms (1Hz)
    
    while (task_running) {
        TickType_t current_time = xTaskGetTickCount();
        
        // MPU6050 읽기 (I2C0 사용) - 초기화된 경우에만
        if (mpu6050_initialized && (current_time - last_mpu6050_time) >= mpu6050_interval) {
            esp_err_t ret = read_sensor_with_retry(read_mpu6050, "MPU6050", 3, true);
            if (ret == ESP_OK) {
                last_mpu6050_time = current_time;
            } else {
                ESP_LOGW(TAG, "MPU6050 읽기 실패 (재시도 중)");
                // 읽기 실패 시에도 다음 주기에서 재시도
            }
        }
        
        // MAX30102 읽기 (I2C1 사용) - 초기화된 경우에만
        if (max30102_initialized && (current_time - last_max30102_time) >= max30102_interval) {
            esp_err_t ret = read_sensor_with_retry(read_max30102, "MAX30102", 3, false);
            if (ret == ESP_OK) {
                last_max30102_time = current_time;
            } else {
                ESP_LOGE(TAG, "MAX30102 읽기 실패");
            }
        }
        
        // MLX90614 읽기 (I2C1 사용) - 초기화된 경우에만
        if (mlx90614_initialized && (current_time - last_mlx90614_time) >= mlx90614_interval) {
            esp_err_t ret = read_sensor_with_retry(read_mlx90614, "MLX90614", 3, false);
            if (ret == ESP_OK) {
                last_mlx90614_time = current_time;
            } else {
                ESP_LOGE(TAG, "MLX90614 읽기 실패");
            }
        }
        
        // 태스크 지연 (1ms)
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    ESP_LOGI(TAG, "센서 매니저 태스크 종료");
    vTaskDelete(NULL);
}

/**
 * @brief 센서 매니저 태스크 시작
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t sensor_manager_start(void) {
    if (task_running) {
        ESP_LOGW(TAG, "센서 매니저 태스크가 이미 실행 중입니다");
        return ESP_OK;
    }
    
    // I2C 뮤텍스 생성
    if (i2c0_mutex == NULL) {
        i2c0_mutex = xSemaphoreCreateMutex();
        if (i2c0_mutex == NULL) {
            ESP_LOGE(TAG, "I2C0 뮤텍스 생성 실패");
            return ESP_FAIL;
        }
    }
    
    if (i2c1_mutex == NULL) {
        i2c1_mutex = xSemaphoreCreateMutex();
        if (i2c1_mutex == NULL) {
            ESP_LOGE(TAG, "I2C1 뮤텍스 생성 실패");
            return ESP_FAIL;
        }
    }
    
    // 센서 초기화 전에 충분한 대기 시간
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // 심박수 계산기 초기화
    heart_rate_calculator_init();
    
    // 센서 초기화
    ESP_LOGI(TAG, "센서 초기화 중...");
    
    // MPU6050 초기화 (I2C0) - 실패 시에도 계속 진행
    esp_err_t ret = mpu6050_init(I2C_MASTER_NUM_0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MPU6050 초기화 실패, 계속 진행: %s", esp_err_to_name(ret));
        mpu6050_initialized = false;
    } else {
        ESP_LOGI(TAG, "MPU6050 초기화 성공");
        mpu6050_initialized = true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // MAX30102 초기화 (I2C1) - 실패 시에도 계속 진행
    ret = max30102_init(I2C_MASTER_NUM_1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MAX30102 초기화 실패, 계속 진행: %s", esp_err_to_name(ret));
        max30102_initialized = false;
    } else {
        ESP_LOGI(TAG, "MAX30102 초기화 성공");
        max30102_initialized = true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // MLX90614 초기화 시도 (I2C1) - 실패 시에도 계속 진행
    ret = mlx90614_init(I2C_MASTER_NUM_1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MLX90614 초기화 실패, 계속 진행: %s", esp_err_to_name(ret));
        mlx90614_initialized = false;
    } else {
        ESP_LOGI(TAG, "MLX90614 초기화 성공");
        mlx90614_initialized = true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 최소한 하나의 센서라도 초기화되었는지 확인
    if (!mpu6050_initialized && !max30102_initialized && !mlx90614_initialized) {
        ESP_LOGW(TAG, "모든 센서 초기화 실패, 하지만 태스크는 시작합니다");
    }
    
    task_running = true;
    
    // 태스크 생성 (스택 크기 증가)
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        sensor_manager_task,
        "sensor_manager_task",
        8192,  // 스택 크기 증가
        NULL,
        configMAX_PRIORITIES - 2,  // 우선순위를 한 단계 낮춤
        &sensor_manager_task_handle,
        1  // Core 1에서 실행
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "센서 매니저 태스크 생성 실패");
        task_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "센서 매니저 태스크 시작됨 (MPU6050: %s, MAX30102: %s, MLX90614: %s)", 
             mpu6050_initialized ? "OK" : "FAIL",
             max30102_initialized ? "OK" : "FAIL", 
             mlx90614_initialized ? "OK" : "FAIL");
    return ESP_OK;
}

/**
 * @brief 센서 매니저 태스크 중지
 */
void sensor_manager_stop(void) {
    if (!task_running) {
        return;
    }
    
    task_running = false;
    
    if (sensor_manager_task_handle != NULL) {
        vTaskDelete(sensor_manager_task_handle);
        sensor_manager_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "센서 매니저 태스크 중지됨");
}
