#include "wifi_connect.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"   

#define WIFI_SSID "eod"
#define WIFI_PASS "dltnwjd00"

static const char *TAG = "WIFI_CONNECT";
static esp_netif_t *s_sta_netif = NULL;

// 이벤트 핸들러
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA 시작, 연결 시도...");
        ESP_LOGI(TAG, "연결 시도 wifi: %s", WIFI_SSID);
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "AP 연결 성공 (SSID: %s)", WIFI_SSID);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "AP 연결 실패, reason=%d → 재시도", disconn->reason);
        esp_wifi_connect();

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP 할당 완료: " IPSTR, IP2STR(&event->ip_info.ip));

        // DNS 주입
        if (s_sta_netif) {
            esp_netif_dns_info_t curr = {0};
            if (esp_netif_get_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &curr) == ESP_OK) {
                bool need_inject = (curr.ip.type != ESP_IPADDR_TYPE_V4) || (curr.ip.u_addr.ip4.addr == 0);

                if (need_inject) {
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

                    ESP_ERROR_CHECK(esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &d1));
                    ESP_ERROR_CHECK(esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_BACKUP, &d2));
                    esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_FALLBACK, &d3);
                }
            }

            // 로그로 최종 DNS 확인
            esp_netif_dns_info_t main_dns = {0};
            if (esp_netif_get_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &main_dns) == ESP_OK) {
                const ip4_addr_t *lwip_ip = (const ip4_addr_t *)&main_dns.ip.u_addr.ip4;
                ESP_LOGI("DNS", "Main DNS after fix: %s", ip4addr_ntoa(lwip_ip));
            }
        }
    }
}
    
void wifi_connect(void)
{
    // NVS 초기화 (Wi-Fi 사용 필수)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 네트워크 인터페이스 초기화
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();

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

    // Wi-Fi 시작
    ESP_ERROR_CHECK(esp_wifi_start());
}
