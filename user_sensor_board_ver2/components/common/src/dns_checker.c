#include "dns_checker.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DNS_CHECKER";

// DNS 테스트 결과를 저장할 구조체
typedef struct {
    char hostname[64];
    char resolved_ip[16];
    bool success;
} dns_test_result_t;

// UDP 123번 포트 테스트 결과를 저장할 구조체
typedef struct {
    char hostname[64];
    char resolved_ip[16];
    bool success;
    int response_time_ms;  // 응답 시간 (밀리초)
} udp_test_result_t;

#define MAX_DNS_TESTS 5
#define MAX_UDP_TESTS 5
static dns_test_result_t dns_results[MAX_DNS_TESTS];
static udp_test_result_t udp_results[MAX_UDP_TESTS];
static int dns_test_count = 0;
static int udp_test_count = 0;

void test_dns(void)
{
    const char *hostname = "google.com";
    struct hostent *h = gethostbyname(hostname);
    if (h && h->h_length > 0) {
        char ipstr[16];
        inet_ntop(AF_INET, h->h_addr_list[0], ipstr, sizeof(ipstr));
        ESP_LOGI(TAG, "DNS OK: %s → %s", hostname, ipstr);
    } else {
        ESP_LOGW(TAG, "DNS FAIL: %s 해석 불가", hostname);
    }
}

esp_err_t test_dns_hostname(const char *hostname)
{
    if (!hostname) {
        ESP_LOGE(TAG, "호스트명이 NULL입니다");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "DNS 테스트 시작: %s", hostname);
    
    struct hostent *h = gethostbyname(hostname);
    if (h && h->h_length > 0) {
        char ipstr[16];
        inet_ntop(AF_INET, h->h_addr_list[0], ipstr, sizeof(ipstr));
        ESP_LOGI(TAG, "DNS OK: %s → %s", hostname, ipstr);
        
        // 결과 저장
        if (dns_test_count < MAX_DNS_TESTS) {
            strncpy(dns_results[dns_test_count].hostname, hostname, sizeof(dns_results[dns_test_count].hostname) - 1);
            strncpy(dns_results[dns_test_count].resolved_ip, ipstr, sizeof(dns_results[dns_test_count].resolved_ip) - 1);
            dns_results[dns_test_count].success = true;
            dns_test_count++;
        }
        
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "DNS FAIL: %s 해석 불가", hostname);
        
        // 실패 결과 저장
        if (dns_test_count < MAX_DNS_TESTS) {
            strncpy(dns_results[dns_test_count].hostname, hostname, sizeof(dns_results[dns_test_count].hostname) - 1);
            dns_results[dns_test_count].resolved_ip[0] = '\0';
            dns_results[dns_test_count].success = false;
            dns_test_count++;
        }
        
        return ESP_FAIL;
    }
}

void test_multiple_dns(void)
{
    ESP_LOGI(TAG, "다중 DNS 테스트 시작");
    
    // 테스트할 호스트명들
    const char *hostnames[] = {
        "google.com",
        "kr.pool.ntp.org",
        "time.google.com",
        "pool.ntp.org",
        "time.windows.com"
    };
    
    int hostname_count = sizeof(hostnames) / sizeof(hostnames[0]);
    
    for (int i = 0; i < hostname_count; i++) {
        test_dns_hostname(hostnames[i]);
        // 각 테스트 사이에 약간의 지연
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "다중 DNS 테스트 완료");
}

void print_dns_test_results(void)
{
    ESP_LOGI(TAG, "=== DNS 테스트 결과 ===");
    ESP_LOGI(TAG, "총 테스트 수: %d", dns_test_count);
    
    int success_count = 0;
    for (int i = 0; i < dns_test_count; i++) {
        if (dns_results[i].success) {
            ESP_LOGI(TAG, "[%d] SUCCESS: %s → %s", i + 1, 
                     dns_results[i].hostname, dns_results[i].resolved_ip);
            success_count++;
        } else {
            ESP_LOGW(TAG, "[%d] FAILED: %s", i + 1, dns_results[i].hostname);
        }
    }
    
    ESP_LOGI(TAG, "성공률: %d/%d (%.1f%%)", 
             success_count, dns_test_count, 
             (float)success_count / dns_test_count * 100.0f);
    ESP_LOGI(TAG, "=====================");
}

esp_err_t test_udp_port_123(const char *hostname)
{
    if (!hostname) {
        ESP_LOGE(TAG, "호스트명이 NULL입니다");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "UDP 123번 포트 테스트 시작: %s", hostname);
    
    // DNS 해석
    struct hostent *h = gethostbyname(hostname);
    if (!h || h->h_length == 0) {
        ESP_LOGW(TAG, "DNS 해석 실패: %s", hostname);
        return ESP_FAIL;
    }
    
    char ipstr[16];
    inet_ntop(AF_INET, h->h_addr_list[0], ipstr, sizeof(ipstr));
    ESP_LOGI(TAG, "DNS 해석 성공: %s → %s", hostname, ipstr);
    
    // UDP 소켓 생성
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "UDP 소켓 생성 실패");
        return ESP_FAIL;
    }
    
    // 소켓 타임아웃 설정 (5초)
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(123);  // NTP 포트
    server_addr.sin_addr.s_addr = *(uint32_t*)h->h_addr_list[0];
    
    // 간단한 NTP 요청 패킷 생성 (최소한의 NTP 헤더)
    uint8_t ntp_request[48] = {0};
    ntp_request[0] = 0x1B;  // LI=0, Version=3, Mode=3 (Client)
    
    // 시작 시간 기록
    int64_t start_time = esp_timer_get_time();
    
    // UDP 패킷 전송
    int sent = sendto(sock, ntp_request, sizeof(ntp_request), 0, 
                     (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (sent < 0) {
        ESP_LOGW(TAG, "UDP 패킷 전송 실패: %s", strerror(errno));
        close(sock);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "UDP 패킷 전송 성공 (%d bytes)", sent);
    
    // 응답 대기
    uint8_t ntp_response[48];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    int received = recvfrom(sock, ntp_response, sizeof(ntp_response), 0,
                           (struct sockaddr*)&from_addr, &from_len);
    
    // 종료 시간 기록
    int64_t end_time = esp_timer_get_time();
    int response_time_ms = (end_time - start_time) / 1000;
    
    close(sock);
    
    if (received > 0) {
        ESP_LOGI(TAG, "UDP 123번 포트 응답 성공: %s (%d bytes, %dms)", 
                 ipstr, received, response_time_ms);
        
        // 결과 저장
        if (udp_test_count < MAX_UDP_TESTS) {
            strncpy(udp_results[udp_test_count].hostname, hostname, sizeof(udp_results[udp_test_count].hostname) - 1);
            strncpy(udp_results[udp_test_count].resolved_ip, ipstr, sizeof(udp_results[udp_test_count].resolved_ip) - 1);
            udp_results[udp_test_count].success = true;
            udp_results[udp_test_count].response_time_ms = response_time_ms;
            udp_test_count++;
        }
        
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "UDP 123번 포트 응답 실패: %s (타임아웃 또는 연결 거부)", ipstr);
        
        // 실패 결과 저장
        if (udp_test_count < MAX_UDP_TESTS) {
            strncpy(udp_results[udp_test_count].hostname, hostname, sizeof(udp_results[udp_test_count].hostname) - 1);
            strncpy(udp_results[udp_test_count].resolved_ip, ipstr, sizeof(udp_results[udp_test_count].resolved_ip) - 1);
            udp_results[udp_test_count].success = false;
            udp_results[udp_test_count].response_time_ms = -1;
            udp_test_count++;
        }
        
        return ESP_FAIL;
    }
}

void test_multiple_udp_port_123(void)
{
    ESP_LOGI(TAG, "다중 UDP 123번 포트 테스트 시작");
    
    // 테스트할 NTP 서버들
    const char *ntp_servers[] = {
        "kr.pool.ntp.org",
        "time.google.com",
        "pool.ntp.org",
        "time.windows.com",
        "time.nist.gov"
    };
    
    int server_count = sizeof(ntp_servers) / sizeof(ntp_servers[0]);
    
    for (int i = 0; i < server_count; i++) {
        ESP_LOGI(TAG, "테스트 %d/%d: %s", i + 1, server_count, ntp_servers[i]);
        test_udp_port_123(ntp_servers[i]);
        // 각 테스트 사이에 지연
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "다중 UDP 123번 포트 테스트 완료");
}

void print_udp_port_123_results(void)
{
    ESP_LOGI(TAG, "=== UDP 123번 포트 테스트 결과 ===");
    ESP_LOGI(TAG, "총 테스트 수: %d", udp_test_count);
    
    int success_count = 0;
    int total_response_time = 0;
    int valid_response_count = 0;
    
    for (int i = 0; i < udp_test_count; i++) {
        if (udp_results[i].success) {
            ESP_LOGI(TAG, "[%d] SUCCESS: %s → %s (%dms)", i + 1, 
                     udp_results[i].hostname, udp_results[i].resolved_ip, 
                     udp_results[i].response_time_ms);
            success_count++;
            total_response_time += udp_results[i].response_time_ms;
            valid_response_count++;
        } else {
            ESP_LOGW(TAG, "[%d] FAILED: %s → %s", i + 1, 
                     udp_results[i].hostname, udp_results[i].resolved_ip);
        }
    }
    
    ESP_LOGI(TAG, "성공률: %d/%d (%.1f%%)", 
             success_count, udp_test_count, 
             (float)success_count / udp_test_count * 100.0f);
    
    if (valid_response_count > 0) {
        float avg_response_time = (float)total_response_time / valid_response_count;
        ESP_LOGI(TAG, "평균 응답 시간: %.1fms", avg_response_time);
    }
    
    ESP_LOGI(TAG, "================================");
}
