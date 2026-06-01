#ifndef ESP8266_ACTIVITY_HPP
#define ESP8266_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

void esp8266_test_activity(void);
void esp8266_scan_activity(void);
void esp8266_http_get_activity(void);
void esp8266_tcp_server_test(void);

#ifdef __cplusplus
}
#endif

#endif // ESP8266_ACTIVITY_HPP