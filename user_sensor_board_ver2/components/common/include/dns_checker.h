#ifndef DNS_CHECKER_H
#define DNS_CHECKER_H

#include "esp_err.h"

// DNS 테스트 함수
void test_dns(void);

// 특정 호스트명으로 DNS 테스트
esp_err_t test_dns_hostname(const char *hostname);

// 여러 호스트명으로 DNS 테스트
void test_multiple_dns(void);

// DNS 테스트 결과 출력
void print_dns_test_results(void);

// UDP 123번 포트 연결 테스트 (SNTP/NTP)
esp_err_t test_udp_port_123(const char *hostname);

// 여러 NTP 서버의 UDP 123번 포트 연결 테스트
void test_multiple_udp_port_123(void);

// UDP 123번 포트 테스트 결과 출력
void print_udp_port_123_results(void);

#endif // DNS_CHECKER_H
