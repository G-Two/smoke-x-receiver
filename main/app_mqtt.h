#ifndef APP_MQTT_H
#define APP_MQTT_H

#define APP_MQTT_URI "uri"
#define APP_MQTT_USERNAME "username"
#define APP_MQTT_PASSWORD "password"
#define APP_MQTT_ENABLED "enabled"
#define APP_MQTT_IDENTITY "identity"
#define APP_MQTT_CA_CERT "ca_cert"
#define APP_MQTT_CLIENT_CERT "client_cert"
#define APP_MQTT_CLIENT_KEY "client_key"
#define APP_MQTT_CERT_AUTH "cert_auth"
#define APP_MQTT_HA_DISCOVERY "ha_discovery"
#define APP_MQTT_HA_BASE_TOPIC "ha_base_topic"
#define APP_MQTT_HA_STATUS_TOPIC "ha_status_topic"
#define APP_MQTT_HA_BIRTH_PAYLOAD "ha_birth_payload"
#define APP_MQTT_STATE_TOPIC "state_topic"

#define APP_MQTT_MAX_URI_LEN 128
#define APP_MQTT_MAX_USERNAME_LEN 128
#define APP_MQTT_MAX_PASSWORD_LEN 128
#define APP_MQTT_MAX_IDENTITY_LEN 128
#define APP_MQTT_MAX_CERT_LEN 2048
#define APP_MQTT_MAX_TOPIC_LEN 48

typedef struct {
    char* uri;
    char* identity;
    char* username;
    char* password;
    char* ca_cert;
    char* client_cert;
    char* client_key;
    bool cert_auth;
    bool enabled;
    bool ha_discovery;
    char* ha_base_topic;
    char* ha_status_topic;
    char* ha_birth_payload;
    char* state_topic;
} app_mqtt_params_t;

/* Live connection/telemetry snapshot for the Device Information page. Ages are
   pre-computed (ms) with -1 meaning "n/a" (never happened / not connected). */
typedef struct {
    bool enabled;
    bool connected;
    bool ha_discovery;
    bool discovery_published;
    int64_t connected_for_ms;    /* since last connect; -1 if not connected */
    int64_t last_publish_ms_ago; /* -1 if nothing published yet */
    uint32_t publish_count;      /* state messages sent while connected */
    uint32_t connect_count;      /* successful connections since boot */
    char last_error[80];         /* last connection error; "" if none */
    int64_t last_error_ms_ago;   /* -1 if no error recorded */
} app_mqtt_stats_t;

esp_err_t app_mqtt_start();
void app_mqtt_stop();
bool app_mqtt_is_connected();
bool app_mqtt_is_enabled();
void app_mqtt_get_stats(app_mqtt_stats_t*);
void app_mqtt_publish_discovery();
void app_mqtt_publish_state();
void app_mqtt_get_params(app_mqtt_params_t*);
esp_err_t app_mqtt_set_params(app_mqtt_params_t*);

#endif
