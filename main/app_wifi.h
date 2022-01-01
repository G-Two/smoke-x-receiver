#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <esp_wifi.h>

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

#endif
