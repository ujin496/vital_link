// wifi_connect.c

#include "wifi_connect.h"
#include "sntp_helper.h"
#include "time_helper.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dns_checker.h"

#define WIFI_SSID "A107"
#define WIFI_PASS "123456789"

static const char *TAG = "WIFI_CONNECT";

// Wi-Fi 연결 상태
static bool wifi_connected = false;
static esp_netif_t* wifi_netif = NULL;

// DNS 정보 확인 함수
static bool check_dns_configuration(void) {
    if (wifi_netif == NULL) {
        ESP_LOGW(TAG, "Wi-Fi 네트워크 인터페이스가 NULL");
        return false;
    }
    
    esp_netif_dns_info_t dns_info;
    esp_err_t ret = esp_netif_get_dns_info(wifi_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "DNS 서버: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        return true;
    } else {
        ESP_LOGW(TAG, "DNS 정보 가져오기 실패: %s", esp_err_to_name(ret));
        return false;
    }
}

// DNS 강제 주입 함수
static void inject_dns_servers(ip_event_got_ip_t* event) {
    if (wifi_netif == NULL) {
        ESP_LOGW(TAG, "Wi-Fi 네트워크 인터페이스가 NULL, DNS 주입 실패");
        return;
    }
    
    esp_netif_dns_info_t curr = {0};
    if (esp_netif_get_dns_info(wifi_netif, ESP_NETIF_DNS_MAIN, &curr) == ESP_OK) {
        bool need_inject = (curr.ip.type != ESP_IPADDR_TYPE_V4) || (curr.ip.u_addr.ip4.addr == 0);
        
        if (need_inject) {
            ESP_LOGI(TAG, "DNS 강제 주입 시작");
            
            // 1) 메인 DNS = 게이트웨이
            esp_netif_dns_info_t d1 = {0};
            d1.ip.type = ESP_IPADDR_TYPE_V4;
            d1.ip.u_addr.ip4.addr = event->ip_info.gw.addr;
            
            // 2) 백업 DNS = 8.8.8.8
            esp_netif_dns_info_t d2 = {0};
            d2.ip.type = ESP_IPADDR_TYPE_V4;
            IP4_ADDR(&d2.ip.u_addr.ip4, 8, 8, 8, 8);
            
            // 3) 폴백 DNS = 1.1.1.1
            esp_netif_dns_info_t d3 = {0};
            d3.ip.type = ESP_IPADDR_TYPE_V4;
            IP4_ADDR(&d3.ip.u_addr.ip4, 1, 1, 1, 1);
            
            ESP_ERROR_CHECK(esp_netif_set_dns_info(wifi_netif, ESP_NETIF_DNS_MAIN, &d1));
            ESP_ERROR_CHECK(esp_netif_set_dns_info(wifi_netif, ESP_NETIF_DNS_BACKUP, &d2));
            esp_netif_set_dns_info(wifi_netif, ESP_NETIF_DNS_FALLBACK, &d3);
            
            ESP_LOGI(TAG, "DNS 강제 주입 완료");
        }
    }
    
    // 로그로 최종 DNS 확인
    esp_netif_dns_info_t main_dns = {0};
    if (esp_netif_get_dns_info(wifi_netif, ESP_NETIF_DNS_MAIN, &main_dns) == ESP_OK) {
        const ip4_addr_t *lwip_ip = (const ip4_addr_t *)&main_dns.ip.u_addr.ip4;
        ESP_LOGI(TAG, "Main DNS after fix: %s", ip4addr_ntoa(lwip_ip));
    }
}

// 네트워크 연결 상태 확인 함수
static bool check_network_connectivity(void) {
    if (!wifi_connected) {
        ESP_LOGW(TAG, "Wi-Fi가 연결되지 않음");
        return false;
    }
    
    if (!check_dns_configuration()) {
        ESP_LOGW(TAG, "DNS 설정 확인 실패");
        return false;
    }
    
    ESP_LOGI(TAG, "네트워크 연결 상태 확인 완료");
    return true;
}

// Wi-Fi 연결 완료 이벤트 핸들러
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA 시작, 연결 시도...");
        ESP_LOGI(TAG, "연결 시도 wifi: %s", WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "AP 연결 성공 (SSID: %s)", WIFI_SSID);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "AP 연결 실패, reason=%d → 재시도", disconn->reason);
        wifi_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Wi-Fi 연결 성공, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "게이트웨이: " IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "넷마스크: " IPSTR, IP2STR(&event->ip_info.netmask));
        
        wifi_connected = true;
        
        // DNS 강제 주입
        inject_dns_servers(event);
        
        // 네트워크 안정화를 위한 대기
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        // 네트워크 연결 상태 확인 후 시간 동기화 시작
        if (check_network_connectivity()) {
            // 별도 태스크에서 시간 동기화 수행
            xTaskCreate(time_sync_task, "time_sync", 4096, NULL, 5, NULL);
        } else {
            ESP_LOGE(TAG, "네트워크 연결 상태 확인 실패, 시간 동기화 건너뜀");
        }
    }
}

// 별도 태스크에서 시간 동기화 수행
static void time_sync_task(void *pvParameters) {
    ESP_LOGI(TAG, "시간 동기화 태스크 시작");
    
    // 네트워크 연결 상태 재확인
    if (!check_network_connectivity()) {
        ESP_LOGE(TAG, "네트워크 연결 상태 확인 실패, 태스크 종료");
        vTaskDelete(NULL);
        return;
    }
    
    // 추가 대기 (네트워크 안정화)
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // // DNS 테스트
    // ESP_LOGI(TAG, "DNS 연결 테스트 시작");
    // test_multiple_dns();
    // print_dns_test_results();
    
    // // UDP 123번 포트 테스트 (NTP 서버 연결 확인)
    // ESP_LOGI(TAG, "UDP 123번 포트 연결 테스트 시작");
    // test_multiple_udp_port_123();
    // print_udp_port_123_results();
    
    // SNTP 시도 (더 긴 타임아웃)
    ESP_LOGI(TAG, "SNTP 시간 동기화 시작");
    esp_err_t ret = sntp_init_and_sync();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SNTP 동기화 완료");
    } else {
        ESP_LOGW(TAG, "SNTP 동기화 실패, 로컬 시간 사용");
        
        // 시간 동기화 실패 시 주기적으로 재시도
        ESP_LOGI(TAG, "시간 동기화 재시도 태스크 시작");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(30000));  // 30초마다 재시도
            
            ESP_LOGI(TAG, "시간 동기화 재시도 중...");
            
            // SNTP 재시도
            ret = sntp_init_and_sync();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "SNTP 재시도 성공");
                break;
            }
            
            ESP_LOGW(TAG, "SNTP 재시도 실패, 30초 후 다시 시도");
        }
    }
    
    vTaskDelete(NULL);
}

void wifi_connect(void) {
    ESP_LOGI(TAG, "Wi-Fi 연결 시작");
    
    // NVS 초기화 (Wi-Fi 사용 필수)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    
    // 초기화
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 이벤트 핸들러 등록
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Wi-Fi 설정
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // WPA2 이상만 연결
        },
    };
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Wi-Fi 연결 시도 중...");
}
