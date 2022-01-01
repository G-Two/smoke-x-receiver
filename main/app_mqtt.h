#ifndef APP_MQTT_H
#define APP_MQTT_H

#define APP_MQTT_MAX_URI_LEN 128
#define APP_MQTT_MAX_USERNAME_LEN 128
#define APP_MQTT_MAX_PASSWORD_LEN 128
#define APP_MQTT_MAX_IDENTITY_LEN 128

typedef struct {
    char* uri;
    char* identity;
    char* username;
    char* password;
    bool enabled;
} app_mqtt_params_t;

esp_err_t app_mqtt_start();
void app_mqtt_stop();
bool app_mqtt_is_connected();
bool app_mqtt_is_enabled();
void app_mqtt_publish_state();
void app_mqtt_get_params(app_mqtt_params_t*);
esp_err_t app_mqtt_set_params(app_mqtt_params_t*);

#endif
