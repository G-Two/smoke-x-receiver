#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <esp_err.h>
#include <esp_wifi.h>
#include <stddef.h>

typedef struct {
    wifi_mode_t mode;
    wifi_auth_mode_t auth_type;
    char ssid[32];
    char username[128];
    char password[64];
    bool valid;
} app_wifi_params_t;

void app_wifi_init();
void app_wifi_get_params(app_wifi_params_t *params);
void app_wifi_set_params(app_wifi_params_t *params);
bool app_wifi_validate_params(app_wifi_params_t *params);
bool app_wifi_is_connected();
bool app_wifi_is_ap_mode();
esp_err_t app_wifi_get_ip_str(char *buf, size_t len);
esp_err_t app_wifi_get_sta_rssi(int8_t *rssi, uint8_t *channel);
esp_err_t app_wifi_try_connect(const char *ssid, const char *password,
                               uint32_t timeout_ms, char *out_ip_str,
                               size_t out_ip_str_len);

#endif
