#include <stdlib.h>
#include <esp_log.h>
#include <esp_system.h>
#include <cJSON.h>
#include <mqtt_client.h>
#include <nvs.h>
#include "app_mqtt.h"
#include "smoke_x.h"

#define NVS_NAMESPACE "mqtt_config"
#define MQTT_BUF_SIZE 512

#define BOOL_TO_STR(b) b ? "ON" : "OFF"

// Home Assistant specific things
// https://www.home-assistant.io/docs/mqtt/discovery/
#define HASS_MQTT_BASE_TOPIC "homeassistant"
#define HASS_MQTT_STATUS_TOPIC "homeassistant/status"
#define HASS_MQTT_HASS_BIRTH "online"
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
                                            .ca_cert = NULL,
                                            .enabled = false,
                                            .ha_discovery = false,
                                            .ha_base_topic = NULL,
                                            .ha_status_topic = NULL,
                                            .ha_birth_payload = NULL,
                                            .state_topic = NULL};
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
            esp_mqtt_client_subscribe(client, app_mqtt_params.ha_status_topic,
                                      1);
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
            if (strcmp(event->topic, app_mqtt_params.ha_status_topic)) {
                if (strcmp(event->data, app_mqtt_params.ha_birth_payload)) {
                    ESP_LOGI(TAG, "Home Assistant MQTT birth message received");
                    if (app_mqtt_params.ha_discovery) {
                        esp_event_post(SMOKE_X_EVENT,
                                       SMOKE_X_EVENT_DISCOVERY_REQUIRED, NULL,
                                       0, 1000);
                    }
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
        err = nvs_get_str(h_nvs, APP_MQTT_URI, NULL, &len);
        if (!err) {
            app_mqtt_params.uri = malloc(len);
            err = nvs_get_str(h_nvs, APP_MQTT_URI, app_mqtt_params.uri, &len);
        }

        err = nvs_get_str(h_nvs, APP_MQTT_USERNAME, NULL, &len);
        if (!err) {
            app_mqtt_params.username = malloc(len);
            err = nvs_get_str(h_nvs, APP_MQTT_USERNAME,
                              app_mqtt_params.username, &len);
        }

        err = nvs_get_str(h_nvs, APP_MQTT_PASSWORD, NULL, &len);
        if (!err) {
            app_mqtt_params.password = malloc(len);
            err = nvs_get_str(h_nvs, APP_MQTT_PASSWORD,
                              app_mqtt_params.password, &len);
        }

        err = nvs_get_str(h_nvs, APP_MQTT_IDENTITY, NULL, &len);
        if (!err) {
            app_mqtt_params.identity = malloc(len);
            err = nvs_get_str(h_nvs, APP_MQTT_IDENTITY,
                              app_mqtt_params.identity, &len);
        }

        err = nvs_get_str(h_nvs, APP_MQTT_CA_CERT, NULL, &len);
        if (!err) {
            app_mqtt_params.ca_cert = malloc(len);
            err = nvs_get_str(h_nvs, APP_MQTT_CA_CERT, app_mqtt_params.ca_cert,
                              &len);
        }

        err = nvs_get_i8(h_nvs, APP_MQTT_ENABLED,
                         (int8_t *)&app_mqtt_params.enabled);
        if (err) {
            app_mqtt_params.enabled = false;
        }

        err = nvs_get_i8(h_nvs, APP_MQTT_HA_DISCOVERY,
                         (int8_t *)&app_mqtt_params.ha_discovery);
        if (err) {
            app_mqtt_params.ha_discovery = false;
        }

        err = nvs_get_str(h_nvs, APP_MQTT_HA_BASE_TOPIC, NULL, &len);
        if (!err) {
            app_mqtt_params.ha_base_topic = malloc(len);
            err = nvs_get_str(h_nvs, APP_MQTT_HA_BASE_TOPIC,
                              app_mqtt_params.ha_base_topic, &len);
        } else {
            app_mqtt_params.ha_base_topic = HASS_MQTT_BASE_TOPIC;
        }

        err = nvs_get_str(h_nvs, APP_MQTT_HA_STATUS_TOPIC, NULL, &len);
        if (!err) {
            app_mqtt_params.ha_status_topic = malloc(len);
            err = nvs_get_str(h_nvs, APP_MQTT_HA_STATUS_TOPIC,
                              app_mqtt_params.ha_status_topic, &len);
        } else {
            app_mqtt_params.ha_status_topic = HASS_MQTT_STATUS_TOPIC;
        }

        err = nvs_get_str(h_nvs, APP_MQTT_HA_BIRTH_PAYLOAD, NULL, &len);
        if (!err) {
            app_mqtt_params.ha_birth_payload = malloc(len);
            err = nvs_get_str(h_nvs, APP_MQTT_HA_BIRTH_PAYLOAD,
                              app_mqtt_params.ha_birth_payload, &len);
        } else {
            app_mqtt_params.ha_birth_payload = HASS_MQTT_HASS_BIRTH;
        }

        err = nvs_get_str(h_nvs, APP_MQTT_STATE_TOPIC, NULL, &len);
        if (!err) {
            app_mqtt_params.state_topic = malloc(len);
            err = nvs_get_str(h_nvs, APP_MQTT_STATE_TOPIC,
                              app_mqtt_params.state_topic, &len);
        } else {
            app_mqtt_params.state_topic = HASS_MQTT_STATE_TOPIC;
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
        err = nvs_set_str(h_nvs, APP_MQTT_URI, app_mqtt_params.uri);
        err = nvs_set_str(h_nvs, APP_MQTT_USERNAME, app_mqtt_params.username);
        err = nvs_set_str(h_nvs, APP_MQTT_PASSWORD, app_mqtt_params.password);
        err = nvs_set_str(h_nvs, APP_MQTT_IDENTITY, app_mqtt_params.identity);
        err = nvs_set_str(h_nvs, APP_MQTT_CA_CERT, app_mqtt_params.ca_cert);
        err = nvs_set_i8(h_nvs, APP_MQTT_ENABLED,
                         (int8_t)app_mqtt_params.enabled);
        err = nvs_set_i8(h_nvs, APP_MQTT_HA_DISCOVERY,
                         (int8_t)app_mqtt_params.ha_discovery);
        err = nvs_set_str(h_nvs, APP_MQTT_HA_BASE_TOPIC,
                          app_mqtt_params.ha_base_topic);
        err = nvs_set_str(h_nvs, APP_MQTT_HA_STATUS_TOPIC,
                          app_mqtt_params.ha_status_topic);
        err = nvs_set_str(h_nvs, APP_MQTT_HA_BIRTH_PAYLOAD,
                          app_mqtt_params.ha_birth_payload);
        err = nvs_set_str(h_nvs, APP_MQTT_STATE_TOPIC,
                          app_mqtt_params.state_topic);
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
            .password = app_mqtt_params.password,
            .cert_pem = app_mqtt_params.ca_cert};

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
        ESP_LOGI(TAG, "Starting MQTT client");
        err = esp_mqtt_client_start(client);
    }

    return err;
}

bool app_mqtt_is_connected() { return connected; }

bool app_mqtt_is_enabled() { return app_mqtt_params.enabled; }

void app_mqtt_stop() {
    if (client) {
        ESP_LOGI(TAG, "Stopping MQTT client");
        esp_mqtt_client_disconnect(client);
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        connected = false;
        client = NULL;
    }
}

void app_mqtt_publish_discovery() {
    char buf[MQTT_BUF_SIZE];
    char topic_str[100];
    smoke_x_config_t config;
    smoke_x_get_config(&config);

    ESP_LOGI(TAG, "Sending Home Assistant MQTT Device Discovery");
    cJSON *root = cJSON_CreateObject();
    cJSON *device = cJSON_AddObjectToObject(root, HASS_DEVICE);
    cJSON_AddStringToObject(device, "name", "Smoke X Receiver");
    cJSON_AddStringToObject(device, "identifiers", smoke_x_get_device_id());
    cJSON_AddStringToObject(device, "sw_version", SMOKE_X_APP_VERSION);
    cJSON_AddStringToObject(device, "model",
                            config.num_probes == 2 ? "X2" : "X4");
    cJSON_AddStringToObject(device, "manufacturer", "ThermoWorks");
    cJSON_AddNumberToObject(root, HASS_EXPIRE_AFTER, 120);
    cJSON_AddStringToObject(root, HASS_PAYLOAD_NOT_AVAIL, "offline");
    cJSON_AddStringToObject(root, HASS_STATE_TOPIC,
                            app_mqtt_params.state_topic);

    cJSON_AddStringToObject(root, HASS_DEVICE_CLASS, "temperature");
    cJSON_AddStringToObject(root, HASS_UNIT_OF_MEASUREMENT,
                            smoke_x_get_units());
    cJSON_AddStringToObject(root, "uniq_id", "smoke-x_billows_target");
    cJSON_AddStringToObject(root, HASS_DEVICE_NAME,
                            "Smoke X Billows Target Temp");
    cJSON_AddStringToObject(root, HASS_VALUE_TEMPLATE,
                            "{{value_json.billows_target}}");
    cJSON_PrintPreallocated(root, buf, 512, false);
    snprintf(topic_str, sizeof(topic_str),
             "%s/sensor/smoke-x_billows_target/config",
             app_mqtt_params.ha_base_topic);
    MQTT_PUBLISH(client, topic_str, buf);
    cJSON_DeleteItemFromObject(root, HASS_DEVICE_CLASS);
    cJSON_DeleteItemFromObject(root, HASS_UNIT_OF_MEASUREMENT);

    char uniq_id[32];
    char device_name[32];
    char template[64];
    for (unsigned int i = 0; i < config.num_probes; i++) {
        snprintf(uniq_id, sizeof(uniq_id), "smoke-x_probe_%d_temp", i + 1);
        snprintf(device_name, sizeof(device_name), "Smoke X Probe %d Temp",
                 i + 1);
        snprintf(template, sizeof(template), "{{value_json.probe_%d_temp}}",
                 i + 1);
        cJSON_AddStringToObject(root, HASS_DEVICE_CLASS, "temperature");
        cJSON_AddStringToObject(root, HASS_UNIT_OF_MEASUREMENT,
                                smoke_x_get_units());
        cJSON_ReplaceItemInObject(root, "uniq_id", cJSON_CreateString(uniq_id));
        cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                                  cJSON_CreateString(device_name));
        cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                                  cJSON_CreateString(template));
        cJSON_PrintPreallocated(root, buf, 512, false);
        snprintf(topic_str, sizeof(topic_str), "%s/sensor/%s/config",
                 app_mqtt_params.ha_base_topic, uniq_id);
        MQTT_PUBLISH(client, topic_str, buf);

        snprintf(uniq_id, sizeof(uniq_id), "smoke-x_probe_%d_max", i + 1);
        snprintf(device_name, sizeof(device_name), "Smoke X Probe %d Max",
                 i + 1);
        snprintf(template, sizeof(template), "{{value_json.probe_%d_max}}",
                 i + 1);
        cJSON_ReplaceItemInObject(root, "uniq_id", cJSON_CreateString(uniq_id));
        cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                                  cJSON_CreateString(device_name));
        cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                                  cJSON_CreateString(template));
        cJSON_PrintPreallocated(root, buf, 512, false);
        snprintf(topic_str, sizeof(topic_str), "%s/sensor/%s/config",
                 app_mqtt_params.ha_base_topic, uniq_id);
        MQTT_PUBLISH(client, topic_str, buf);

        snprintf(uniq_id, sizeof(uniq_id), "smoke-x_probe_%d_min", i + 1);
        snprintf(device_name, sizeof(device_name), "Smoke X Probe %d Min",
                 i + 1);
        snprintf(template, sizeof(template), "{{value_json.probe_%d_min}}",
                 i + 1);
        cJSON_ReplaceItemInObject(root, "uniq_id", cJSON_CreateString(uniq_id));
        cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                                  cJSON_CreateString(device_name));
        cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                                  cJSON_CreateString(template));
        cJSON_PrintPreallocated(root, buf, 512, false);
        snprintf(topic_str, sizeof(topic_str), "%s/sensor/%s/config",
                 app_mqtt_params.ha_base_topic, uniq_id);
        MQTT_PUBLISH(client, topic_str, buf);

        snprintf(uniq_id, sizeof(uniq_id), "smoke-x_probe_%d_attached", i + 1);
        snprintf(device_name, sizeof(device_name), "Smoke X Probe %d Attached",
                 i + 1);
        snprintf(template, sizeof(template), "{{value_json.probe_%d_attached}}",
                 i + 1);
        cJSON_ReplaceItemInObject(root, HASS_DEVICE_CLASS,
                                  cJSON_CreateString("plug"));
        cJSON_DeleteItemFromObject(root, HASS_UNIT_OF_MEASUREMENT);
        cJSON_ReplaceItemInObject(root, "uniq_id", cJSON_CreateString(uniq_id));
        cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                                  cJSON_CreateString(device_name));
        cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                                  cJSON_CreateString(template));
        cJSON_PrintPreallocated(root, buf, 512, false);
        snprintf(topic_str, sizeof(topic_str), "%s/binary_sensor/%s/config",
                 app_mqtt_params.ha_base_topic, uniq_id);
        MQTT_PUBLISH(client, topic_str, buf);

        snprintf(uniq_id, sizeof(uniq_id), "smoke-x_probe_%d_alarm", i + 1);
        snprintf(device_name, sizeof(device_name), "Smoke X Probe %d Alarm",
                 i + 1);
        snprintf(template, sizeof(template), "{{value_json.probe_%d_alarm}}",
                 i + 1);
        cJSON_DeleteItemFromObject(root, HASS_DEVICE_CLASS);
        cJSON_ReplaceItemInObject(root, "uniq_id", cJSON_CreateString(uniq_id));
        cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                                  cJSON_CreateString(device_name));
        cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                                  cJSON_CreateString(template));
        cJSON_PrintPreallocated(root, buf, 512, false);
        snprintf(topic_str, sizeof(topic_str), "%s/binary_sensor/%s/config",
                 app_mqtt_params.ha_base_topic, uniq_id);
        MQTT_PUBLISH(client, topic_str, buf);
    }

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_billows_attached"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X Billows Attached"));
    cJSON_AddStringToObject(root, HASS_DEVICE_CLASS, "plug");
    cJSON_ReplaceItemInObject(
        root, HASS_VALUE_TEMPLATE,
        cJSON_CreateString("{{value_json.billows_attached}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    snprintf(topic_str, sizeof(topic_str),
             "%s/binary_sensor/smoke-x_billows_attached/config",
             app_mqtt_params.ha_base_topic);
    MQTT_PUBLISH(client, topic_str, buf);

#ifdef SMOKE_X_DEBUG
    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_debug_heap_free"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X Heap Free"));
    cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                              cJSON_CreateString("{{value_json.heap_free}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    snprintf(topic_str, sizeof(topic_str),
             "%s/sensor/smoke-x_debug_heap_free/config",
             app_mqtt_params.ha_base_topic);
    MQTT_PUBLISH(client, topic_str, buf);

    cJSON_ReplaceItemInObject(root, "uniq_id",
                              cJSON_CreateString("smoke-x_debug_num_records"));
    cJSON_ReplaceItemInObject(root, HASS_DEVICE_NAME,
                              cJSON_CreateString("Smoke X Num Records"));
    cJSON_ReplaceItemInObject(root, HASS_VALUE_TEMPLATE,
                              cJSON_CreateString("{{value_json.num_records}}"));
    cJSON_PrintPreallocated(root, buf, 512, false);
    snprintf(topic_str, sizeof(topic_str),
             "%s/sensor/smoke-x_debug_num_records/config",
             app_mqtt_params.ha_base_topic);
    MQTT_PUBLISH(client, topic_str, buf);
#endif

    cJSON_Delete(root);
    discovery_published = true;
}

void app_mqtt_publish_state() {
    char buf[MQTT_BUF_SIZE];
    smoke_x_state_t state;
    cJSON *root;
    char tmp_str[32];

    if (!discovery_published && app_mqtt_params.ha_discovery) {
        esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_DISCOVERY_REQUIRED, NULL, 0,
                       1000);
    }

    smoke_x_get_state(&state);
    root = cJSON_CreateObject();

    for (unsigned int i = 0; i < state.num_probes; i++) {
        snprintf(tmp_str, sizeof(tmp_str), "probe_%d_attached", i + 1);
        cJSON_AddStringToObject(root, tmp_str,
                                BOOL_TO_STR(state.probes[i].attached));
        if (state.probes[i].attached) {
            snprintf(tmp_str, sizeof(tmp_str), "probe_%d_alarm", i + 1);
            cJSON_AddStringToObject(root, tmp_str,
                                    BOOL_TO_STR(state.probes[i].alarm));

            snprintf(tmp_str, sizeof(tmp_str), "probe_%d_temp", i + 1);
            cJSON_AddNumberToObject(root, tmp_str, state.probes[i].temp);

            snprintf(tmp_str, sizeof(tmp_str), "probe_%d_max", i + 1);
            cJSON_AddNumberToObject(root, tmp_str, state.probes[i].max_temp);

            snprintf(tmp_str, sizeof(tmp_str), "probe_%d_min", i + 1);
            cJSON_AddNumberToObject(root, tmp_str, state.probes[i].min_temp);

            if ((i == (state.num_probes - 1)) && state.billows_attached) {
                cJSON_AddNumberToObject(root, "billows_target",
                                        state.probes[i].billows_target);
                snprintf(tmp_str, sizeof(tmp_str), "probe_%d_max", i + 1);
                cJSON_ReplaceItemInObject(root, tmp_str,
                                          cJSON_CreateString("offline"));

                snprintf(tmp_str, sizeof(tmp_str), "probe_%d_min", i + 1);
                cJSON_ReplaceItemInObject(root, tmp_str,
                                          cJSON_CreateString("offline"));
            } else {
                cJSON_AddStringToObject(root, "billows_target", "offline");
            }
        } else {
            snprintf(tmp_str, sizeof(tmp_str), "probe_%d_alarm", i + 1);
            cJSON_AddStringToObject(root, tmp_str, "offline");

            snprintf(tmp_str, sizeof(tmp_str), "probe_%d_temp", i + 1);
            cJSON_AddStringToObject(root, tmp_str, "offline");

            snprintf(tmp_str, sizeof(tmp_str), "probe_%d_max", i + 1);
            cJSON_AddStringToObject(root, tmp_str, "offline");

            snprintf(tmp_str, sizeof(tmp_str), "probe_%d_min", i + 1);
            cJSON_AddStringToObject(root, tmp_str, "offline");
        }
    }
    cJSON_AddStringToObject(root, "billows_attached",
                            BOOL_TO_STR(state.billows_attached));

#ifdef SMOKE_X_DEBUG
    cJSON_AddNumberToObject(root, "heap_free", xPortGetFreeHeapSize());
    cJSON_AddNumberToObject(root, "num_records", smoke_x_get_num_records());
#endif

    cJSON_PrintPreallocated(root, buf, 512, false);
    MQTT_PUBLISH(client, app_mqtt_params.state_topic, buf);
    cJSON_Delete(root);
}

void app_mqtt_get_params(app_mqtt_params_t *params) {
    memcpy(params, &app_mqtt_params, sizeof(app_mqtt_params_t));
}

esp_err_t app_mqtt_set_params(app_mqtt_params_t *params) {
    esp_err_t err;

    if (params->uri && params->username && params->password &&
        params->identity && params->ca_cert) {
        memcpy(&app_mqtt_params, params, sizeof(app_mqtt_params_t));
        err = save_config_to_nvs();
        update_client_status();
        return err;
    }
    return ESP_FAIL;
}
