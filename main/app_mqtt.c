#include <stdlib.h>
#include <esp_log.h>
#include <esp_system.h>
#include <cJSON.h>
#include <mqtt_client.h>
#include <nvs.h>
#include "app_mqtt.h"
#include "smoke_x.h"

#define NVS_NAMESPACE "mqtt_config"
#define NVS_URI "uri"
#define NVS_USERNAME "username"
#define NVS_PASSWORD "password"
#define NVS_ENABLED "enabled"
#define NVS_IDENTITY "identity"
#define MQTT_BUF_SIZE 512

#define BOOL_TO_STR(b) b ? "ON" : "OFF"

// Home Assistant specific things
// https://www.home-assistant.io/docs/mqtt/discovery/
#define HASS_MQTT_STATUS_TOPIC "homeassistant/status"
#define HASS_MQTT_HASS_BIRTH "online"
#define HASS_MQTT_CONFIG_TOPIC "homeassistant/sensor/smoke-x/config"
#define HASS_MQTT_STATE_TOPIC "homeassistant/smoke-x/state"
#define HASS_DEVICE "dev"
#define HASS_DEVICE_CLASS "dev_cla"
#define HASS_DEVICE_NAME "name"
#define HASS_EXPIRE_AFTER "exp_aft"
#define HASS_PAYLOAD_NOT_AVAIL "pl_not_avail"
#define HASS_STATE_TOPIC "stat_t"
#define HASS_UNIT_OF_MEASUREMENT "unit_of_meas"
#define HASS_VALUE_TEMPLATE "val_tpl"

static app_mqtt_params_t app_mqtt_params = {.uri = NULL,
                                            .identity = NULL,
                                            .username = NULL,
                                            .password = NULL,
                                            .enabled = false};
static esp_mqtt_client_handle_t client = NULL;
static bool connected = false;
static const char *TAG = "app_mqtt";
static bool discovery_published;

#define MQTT_PUBLISH(client, topic, buf)                            \
    if (esp_mqtt_client_enqueue(client, topic, buf,                 \
                                strnlen(buf, MQTT_BUF_SIZE), 1, 0,  \
                                0) == ESP_FAIL) {                   \
        ESP_LOGE(TAG, "Failed to send message to server: %s", buf); \
    }

static void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base,
             event_id);
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(client, HASS_MQTT_STATUS_TOPIC, 1);
            connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            connected = false;
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "MQTT_EVENT_DATA %s:%s", event->topic, event->data);
            if (strcmp(event->topic, HASS_MQTT_STATUS_TOPIC)) {
                if (strcmp(event->data, HASS_MQTT_HASS_BIRTH)) {
                    ESP_LOGI(TAG, "Home Assistant MQTT birth message received");
                    esp_event_post(SMOKE_X_EVENT,
                                   SMOKE_X_EVENT_DISCOVERY_REQUIRED, NULL, 0,
                                   1000);
                }
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type ==
                MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls",
                                     event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack",
                                     event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero(
                    "captured as transport's socket errno",
                    event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(
                    TAG, "Last errno string (%s)",
                    strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static esp_err_t load_config_from_nvs() {
    esp_err_t err;
    nvs_handle_t h_nvs;
    size_t len = 0;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h_nvs);
    if (!err) {
        err = nvs_get_str(h_nvs, NVS_URI, NULL, &len);
        if (!err) {
            app_mqtt_params.uri = malloc(len);
            err = nvs_get_str(h_nvs, NVS_URI, app_mqtt_params.uri, &len);
        }

        err = nvs_get_str(h_nvs, NVS_USERNAME, NULL, &len);
        if (!err) {
            app_mqtt_params.username = malloc(len);
            err = nvs_get_str(h_nvs, NVS_USERNAME, app_mqtt_params.username,
                              &len);
        }

        err = nvs_get_str(h_nvs, NVS_PASSWORD, NULL, &len);
        if (!err) {
            app_mqtt_params.password = malloc(len);
            err = nvs_get_str(h_nvs, NVS_PASSWORD, app_mqtt_params.password,
                              &len);
        }

        err = nvs_get_str(h_nvs, NVS_IDENTITY, NULL, &len);
        if (!err) {
            app_mqtt_params.identity = malloc(len);
            err = nvs_get_str(h_nvs, NVS_IDENTITY, app_mqtt_params.identity,
                              &len);
        }

        err =
            nvs_get_i8(h_nvs, NVS_ENABLED, (int8_t *)&app_mqtt_params.enabled);
        if (err) {
            app_mqtt_params.enabled = false;
        }
        nvs_close(h_nvs);
    }
    return err;
}

static esp_err_t save_config_to_nvs() {
    esp_err_t err;
    nvs_handle_t h_nvs;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h_nvs);
    if (!err) {
        err = nvs_set_str(h_nvs, NVS_URI, app_mqtt_params.uri);
        err = nvs_set_str(h_nvs, NVS_USERNAME, app_mqtt_params.username);
        err = nvs_set_str(h_nvs, NVS_PASSWORD, app_mqtt_params.password);
        err = nvs_set_str(h_nvs, NVS_IDENTITY, app_mqtt_params.identity);
        err = nvs_set_i8(h_nvs, NVS_ENABLED, (int8_t)app_mqtt_params.enabled);
        nvs_close(h_nvs);
    }
    return err;
}

static void update_client_status() {
    if (app_mqtt_params.enabled) {
        if (client) {
            app_mqtt_stop();
        }
        app_mqtt_start();
    } else {
        if (client) {
            app_mqtt_stop();
        }
    }
}

static esp_err_t init() {
    esp_err_t err;

    err = load_config_from_nvs();

    if (!err & app_mqtt_params.enabled) {
        esp_mqtt_client_config_t mqtt_cfg = {
            .uri = app_mqtt_params.uri,
            .client_id = app_mqtt_params.identity,
            .username = app_mqtt_params.username,
            .password = app_mqtt_params.password};

        client = esp_mqtt_client_init(&mqtt_cfg);
        err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                             mqtt_event_handler, NULL);
    }
    return err;
}

esp_err_t app_mqtt_start() {
    esp_err_t err;

    err = init();
    if (!err & app_mqtt_params.enabled) {
        err = esp_mqtt_client_start(client);
    }

    return err;
}

bool app_mqtt_is_connected() { return connected; }

bool app_mqtt_is_enabled() { return app_mqtt_params.enabled; }

void app_mqtt_stop() {
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }
}

void app_mqtt_publish_discovery() {
    char buf[MQTT_BUF_SIZE];

    ESP_LOGI(TAG, "Sending Home Assistant MQTT Device Discovery");
    cJSON *root = cJSON_CreateObject();
    cJSON *device = cJSON_AddObjectToObject(root, HASS_DEVICE);
    cJSON_AddStringToObject(device, "name", "Smoke X2 Receiver");
    cJSON_AddStringToObject(device, "identifiers", smoke_x_get_device_id());
    cJSON_AddStringToObject(device, "sw_version", SMOKE_X_APP_VERSION);
    cJSON_AddStringToObject(device, "model", "Smoke X2");
    cJSON_AddStringToObject(device, "manufacturer", "ThermoWorks");

    cJSON_AddStringToObject(root, HASS_DEVICE_CLASS, "temperature");
    cJSON_AddStringToObject(root, "uniq_id", "smoke-x_probe_1_temp");
    cJSON_AddStringToObject(root, HASS_DEVICE_NAME, "Smoke X2 Probe 1 Temp");
    cJSON_AddStringToObject(root, HASS_STATE_TOPIC, HASS_MQTT_STATE_TOPIC);
    cJSON_AddStringToObject(root, HASS_PAYLOAD_NOT_AVAIL, "offline");
    cJSON_AddStringToObject(root, HASS_UNIT_OF_MEASUREMENT,
                            smoke_x_get_units());
    cJSON_AddStringToObject(root, HASS_VALUE_TEMPLATE,
                            "{{value_json.probe_1_temp}}");
    cJSON_AddNumberToObject(root, HASS_EXPIRE_AFTER, 120);
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client, "homeassistant/sensor/smoke-x_probe_1_temp/config",
                 buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_probe_2_temp"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Probe 2 Temp"));
    cJSON_ReplaceItemInObject(
        root, HASS_VALUE_TEMPLATE,
        cJSON_CreateString("{{value_json.probe_2_temp}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client, "homeassistant/sensor/smoke-x_probe_2_temp/config",
                 buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_probe_1_max"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Probe 1 Max"));
    cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                              cJSON_CreateString("{{value_json.probe_1_max}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client, "homeassistant/sensor/smoke-x_probe_1_max/config",
                 buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_probe_1_min"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Probe 1 Min"));
    cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                              cJSON_CreateString("{{value_json.probe_1_min}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client, "homeassistant/sensor/smoke-x_probe_1_min/config",
                 buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_probe_2_max"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Probe 2 Max"));
    cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                              cJSON_CreateString("{{value_json.probe_2_max}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client, "homeassistant/sensor/smoke-x_probe_2_max/config",
                 buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_probe_2_min"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Probe 2 Min"));
    cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                              cJSON_CreateString("{{value_json.probe_2_min}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client, "homeassistant/sensor/smoke-x_probe_2_min/config",
                 buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_billows_target"));
    cJSON_ReplaceItemInObject(
        root, HASS_DEVICE_NAME,
        cJSON_CreateString("Smoke X2 Billows Target Temp"));
    cJSON_ReplaceItemInObject(
        root, HASS_VALUE_TEMPLATE,
        cJSON_CreateString("{{value_json.billows_target}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client, "homeassistant/sensor/smoke-x_billows_target/config",
                 buf);

    cJSON_ReplaceItemInObject(root, HASS_DEVICE_CLASS,
                              cJSON_CreateString("plug"));
    cJSON_DeleteItemFromObject(root, HASS_UNIT_OF_MEASUREMENT);
    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_probe_1_attached"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Probe 1 Attached"));
    cJSON_ReplaceItemInObject(
        root, HASS_VALUE_TEMPLATE,
        cJSON_CreateString("{{value_json.probe_1_attached}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client,
                 "homeassistant/binary_sensor/smoke-x_probe_1_attached/config",
                 buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_probe_2_attached"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Probe 2 Attached"));
    cJSON_ReplaceItemInObject(
        root, HASS_VALUE_TEMPLATE,
        cJSON_CreateString("{{value_json.probe_2_attached}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client,
                 "homeassistant/binary_sensor/smoke-x_probe_2_attached/config",
                 buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_billows_attached"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Billows Attached"));
    cJSON_ReplaceItemInObject(
        root, HASS_VALUE_TEMPLATE,
        cJSON_CreateString("{{value_json.billows_attached}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client,
                 "homeassistant/binary_sensor/smoke-x_billows_attached/config",
                 buf);

    cJSON_DeleteItemFromObject(root, HASS_DEVICE_CLASS);
    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_probe_1_alarm"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Probe 1 Alarm"));
    cJSON_ReplaceItemInObject(
        root, HASS_VALUE_TEMPLATE,
        cJSON_CreateString("{{value_json.probe_1_alarm}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client,
                 "homeassistant/binary_sensor/smoke-x_probe_1_alarm/config",
                 buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_probe_2_alarm"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X2 Probe 2 Alarm"));
    cJSON_ReplaceItemInObject(
        root, HASS_VALUE_TEMPLATE,
        cJSON_CreateString("{{value_json.probe_2_alarm}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);

    MQTT_PUBLISH(client,
                 "homeassistant/binary_sensor/smoke-x_probe_2_alarm/config",
                 buf);

    cJSON_Delete(root);
    discovery_published = true;
}

void app_mqtt_publish_state() {
    char buf[MQTT_BUF_SIZE];
    smoke_x_state_t state;
    cJSON *root;

    if (!discovery_published) {
        esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_DISCOVERY_REQUIRED, NULL, 0,
                       1000);
    }

    smoke_x_get_state(&state);

    root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "probe_1_attached",
                            BOOL_TO_STR(state.probe_1_attached));
    cJSON_AddStringToObject(root, "probe_2_attached",
                            BOOL_TO_STR(state.probe_2_attached));
    cJSON_AddStringToObject(root, "billows_attached",
                            BOOL_TO_STR(state.billows_attached));
    if (state.probe_1_attached) {
        cJSON_AddStringToObject(root, "probe_1_alarm",
                                BOOL_TO_STR(state.probe_1_alarm));
        cJSON_AddNumberToObject(root, "probe_1_temp", state.probe_1_temp);
        cJSON_AddNumberToObject(root, "probe_1_max", state.probe_1_max);
        cJSON_AddNumberToObject(root, "probe_1_min", state.probe_1_min);
    } else {
        cJSON_AddStringToObject(root, "probe_1_alarm", "offline");
        cJSON_AddStringToObject(root, "probe_1_temp", "offline");
        cJSON_AddStringToObject(root, "probe_1_max", "offline");
        cJSON_AddStringToObject(root, "probe_1_min", "offline");
    }
    if (state.probe_2_attached) {
        cJSON_AddStringToObject(root, "probe_2_alarm",
                                BOOL_TO_STR(state.probe_2_alarm));
        cJSON_AddNumberToObject(root, "probe_2_temp", state.probe_2_temp);
        cJSON_AddStringToObject(root, "probe_2_alarm",
                                BOOL_TO_STR(state.probe_2_alarm));

        if (state.billows_attached) {
            cJSON_AddNumberToObject(root, "billows_target",
                                    state.billows_target);
            cJSON_AddStringToObject(root, "probe_2_max", "offline");
            cJSON_AddStringToObject(root, "probe_2_min", "offline");
        } else {
            cJSON_AddNumberToObject(root, "probe_2_max", state.probe_2_max);
            cJSON_AddNumberToObject(root, "probe_2_min", state.probe_2_min);
            cJSON_AddStringToObject(root, "billows_target", "offline");
        }
    } else {
        cJSON_AddStringToObject(root, "probe_2_alarm", "offline");
        cJSON_AddStringToObject(root, "probe_2_temp", "offline");
        cJSON_AddStringToObject(root, "probe_2_max", "offline");
        cJSON_AddStringToObject(root, "probe_2_min", "offline");
        cJSON_AddStringToObject(root, "probe_2_alarm", "offline");
        cJSON_AddStringToObject(root, "billows_target", "offline");
    }

    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client, HASS_MQTT_STATE_TOPIC, buf);

    cJSON_Delete(root);
}

void app_mqtt_get_params(app_mqtt_params_t *params) {
    memcpy(params, &app_mqtt_params, sizeof(app_mqtt_params_t));
}

esp_err_t app_mqtt_set_params(app_mqtt_params_t *params) {
    esp_err_t err;

    if (params->uri && params->username && params->password &&
        params->identity) {
        memcpy(&app_mqtt_params, params, sizeof(app_mqtt_params_t));
        err = save_config_to_nvs();
        update_client_status();
        return err;
    }
    return ESP_FAIL;
}
