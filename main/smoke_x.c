#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs.h>
#include "cJSON.h"
#include "app_lora.h"
#include "smoke_x.h"

#define SMOKE_X2_SYNC_FREQ 920000000
#define SMOKE_X4_SYNC_FREQ 915000000
#define SMOKE_X_RF_MIN 902000000
#define SMOKE_X_RF_MAX 928000000
#define SMOKE_X_NVS_NAMESPACE "smoke_x"
#define SMOKE_X_NVS_CONFIG "config"
#define NUM_COMMAS_SYNC_MSG 6
#define NUM_COMMAS_SUCCESS_MSG 2
#define NUM_COMMAS_X2_STATE_MSG 16
#define NUM_COMMAS_X4_STATE_MSG 26
#define MIN_FREE_HEAP_SIZE 32768
#define MAX_RECORDS 1200
#define JSON_STR_LEN 16000

static const char *TAG = "smoke_x";
static TaskHandle_t xSyncTask = NULL;
static smoke_x_config_t config;
static smoke_x_state_t state;
static bool configured = false;
static bool sync_received = false;
static cJSON *root;
static cJSON *probes[4];
static cJSON *probes_history[4];
static char json_str[JSON_STR_LEN];
static char *probe_names[4] = {SMOKE_X_PROBE_1, SMOKE_X_PROBE_2,
                               SMOKE_X_PROBE_3, SMOKE_X_PROBE_4};

ESP_EVENT_DEFINE_BASE(SMOKE_X_EVENT);

static void init_history_json() {
    root = cJSON_CreateObject();
    for (unsigned int i = 0; i < config.num_probes; i++) {
        probes[i] = cJSON_AddObjectToObject(root, probe_names[i]);
        cJSON_AddNumberToObject(probes[i], SMOKE_X_CURRENT_TEMP, 0);
        cJSON_AddNumberToObject(probes[i], SMOKE_X_ALARM_MAX, 0);
        cJSON_AddNumberToObject(probes[i], SMOKE_X_ALARM_MIN, 0);
        probes_history[i] = cJSON_CreateArray();
        cJSON_AddItemToObject(probes[i], SMOKE_X_HISTORY, probes_history[i]);
    }
    cJSON_AddBoolToObject(root, SMOKE_X_BILLOWS, state.billows_attached);
}

static esp_err_t set_frequency(unsigned int freq) {
    app_lora_params_t rf_params;
    app_lora_get_params(&rf_params);

    if (freq >= SMOKE_X_RF_MIN && freq <= SMOKE_X_RF_MAX) {
        config.frequency = freq;
        ESP_LOGI(TAG, "Frequency set to: %d MHz", config.frequency);

        rf_params.frequency = config.frequency;
        app_lora_set_params(&rf_params, xTaskGetCurrentTaskHandle());
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Frequency out of range (%d-%d): %d MHz", SMOKE_X_RF_MIN,
                 SMOKE_X_RF_MAX, freq);
        return ESP_FAIL;
    }
}

static unsigned int count_commas(const char *msg, const int len) {
    unsigned int result = 0;
    for (int i = 0; i < len; i++) {
        if (msg[i] == ',') result++;
    }
    return result;
}

static void handle_sync_msg(const char *msg, const int len) {
    unsigned char freq_array[4];
    // Example sync message "020001,|dhHWl,160,32,69,54,"
    sync_received = true;
    ESP_LOGI(TAG, "Received sync message: %s", msg);
    char *tmp = strdup(msg);
    strtok(tmp, ",");
    strncpy(config.device_id, strtok(NULL, ","), SMOKE_X_DEVICE_ID_LEN);
    ESP_LOGI(TAG, "DeviceID set to: %s", config.device_id);
    for (int i = 0; i < 4; i++) {
        freq_array[i] = (char)atoi(strtok(NULL, ","));
    }
    free(tmp);

    config.frequency = *(unsigned int *)freq_array;
    if (config.frequency >= SMOKE_X_RF_MIN &&
        config.frequency <= SMOKE_X_RF_MAX) {
        char response[32];
        snprintf(response, sizeof(response), "%s,SUCCESS,", config.device_id);
        app_lora_tx_msg_t tx_msg = {
            .msg = response,
            .repeat_interval_ms = 0,
            .sending_task = xTaskGetCurrentTaskHandle(),
        };
        set_frequency(config.frequency);
        ESP_LOGI(TAG, "Sending sync acknowledgement to transmitter: %s",
                 tx_msg.msg);
        app_lora_start_tx(&tx_msg);
    } else {
        ESP_LOGE(TAG, "Frequency out of range %d", config.frequency);
        config.frequency = 0;
        sync_received = false;
    }
}

static void update_history() {
    if (!root) {
        init_history_json();
    }

    for (unsigned int i = 0; i < config.num_probes; i++) {
        if ((cJSON_GetArraySize(probes_history[0]) >= MAX_RECORDS) ||
            (xPortGetFreeHeapSize() < MIN_FREE_HEAP_SIZE)) {
            cJSON_DeleteItemFromArray(probes_history[i], 0);
        }
        cJSON_AddItemToArray(probes_history[i],
                             cJSON_CreateNumber(state.probes[i].temp));
    }
}

static void parse_state_msg(const char *msg, smoke_x_state_t *state) {
    char *last_units = state->units;
    char *tmp = strdup(msg);
    state->num_probes = config.num_probes;
    strtok(tmp, ",");   // Not using device ID
    strtok(NULL, ",");  // Not using unknown field
    state->units = atoi(strtok(NULL, ",")) == 1 ? "°F" : "°C";
    state->new_alarm = atoi(strtok(NULL, ","));
    for (unsigned int i = 0; i < config.num_probes; i++) {
        state->probes[i].attached = atoi(strtok(NULL, ",")) == 3 ? false : true;
        state->probes[i].temp = atof(strtok(NULL, ",")) / 10.0;
        state->probes[i].alarm = atoi(strtok(NULL, ","));
        state->probes[i].max_temp = atoi(strtok(NULL, ","));
        state->probes[i].min_temp = atoi(strtok(NULL, ","));
    }
    state->billows_attached = atoi(strtok(NULL, ","));
    strtok(NULL, ",");  // Not using unknown field
    free(tmp);
    update_history();
    if (last_units != state->units) {
        esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_DISCOVERY_REQUIRED, NULL, 0,
                       1000);
    }
}

static esp_err_t save_config_to_nvram() {
    esp_err_t err;
    nvs_handle_t h_nvs;
    err = nvs_open(SMOKE_X_NVS_NAMESPACE, NVS_READWRITE, &h_nvs);
    if (!err) {
        err = nvs_set_blob(h_nvs, SMOKE_X_NVS_CONFIG, &config,
                           sizeof(smoke_x_config_t));
        nvs_close(h_nvs);
    }
    return err;
}

static esp_err_t read_config_from_nvram() {
    esp_err_t err;
    nvs_handle_t h_nvs;
    size_t len = 0;

    err = nvs_open(SMOKE_X_NVS_NAMESPACE, NVS_READWRITE, &h_nvs);
    if (!err) {
        err = nvs_get_blob(h_nvs, SMOKE_X_NVS_CONFIG, NULL, &len);
        err = nvs_get_blob(h_nvs, SMOKE_X_NVS_CONFIG, &config, &len);
        ESP_LOGD(TAG, "nvs_get_blob err: %d", err);
        nvs_close(h_nvs);
        if (ESP_OK == err) {
            if ((config.frequency >= SMOKE_X_RF_MIN &&
                 config.frequency <= SMOKE_X_RF_MAX) &&
                strlen(config.device_id) > 0) {
                ESP_LOGI(TAG, "Device is paired to %s at %d MHz",
                         config.device_id, config.frequency);
                configured = true;
                sync_received = true;
                return ESP_OK;
            }
        }

        ESP_LOGI(TAG, "Device is not paired, waiting for sync on %d",
                 config.frequency);
        configured = false;
        sync_received = false;
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void handle_rx(const char *msg, const int len) {
    switch (count_commas(msg, len)) {
        case NUM_COMMAS_SYNC_MSG:
            if (!configured && !sync_received) {
                handle_sync_msg(msg, len);
                esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_SYNC, NULL, 0,
                               1000);
            } else {
                ESP_LOGI(
                    TAG,
                    "Received unexpected sync message that will be ignored: %s",
                    msg);
            }
            break;
        case NUM_COMMAS_X2_STATE_MSG:
            if (sync_received) {
                if (!configured) {
                    config.num_probes = 2;
                    configured = true;
                    save_config_to_nvram();
                    esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_SYNC_SUCCESS,
                                   NULL, 0, 1000);
                    ESP_LOGI(
                        TAG,
                        "Received data transmission from %s, saving config",
                        config.device_id);
                }
                parse_state_msg(msg, &state);
                ESP_LOGI(TAG, "X2 DATA: %s", msg);
                esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_STATE_MSG_RECEIVED,
                               NULL, 0, 1000);
            }
            break;
        case NUM_COMMAS_X4_STATE_MSG:
            if (sync_received) {
                if (!configured) {
                    config.num_probes = 4;
                    configured = true;
                    save_config_to_nvram();
                    esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_SYNC_SUCCESS,
                                   NULL, 0, 1000);
                    ESP_LOGI(
                        TAG,
                        "Received data transmission from %s, saving config",
                        config.device_id);
                }
                parse_state_msg(msg, &state);
                ESP_LOGI(TAG, "X4 DATA: %s", msg);
                esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_STATE_MSG_RECEIVED,
                               NULL, 0, 1000);
            }
            break;
        default:
            ESP_LOGE(TAG, "Received unrecognized message type: %s", msg);
            break;
    }
}

static void sync_task(void *pvParameter) {
    ESP_LOGI(TAG, "Starting Smoke X Sync");
    int target_freq = SMOKE_X2_SYNC_FREQ;
    while (1) {
        if (!configured && !sync_received) {
            target_freq = SMOKE_X2_SYNC_FREQ == target_freq
                              ? SMOKE_X4_SYNC_FREQ
                              : SMOKE_X2_SYNC_FREQ;
            set_frequency(target_freq);
        }
        vTaskDelay(pdMS_TO_TICKS(3300));
    }
}

esp_err_t start_sync() {
    if (!xSyncTask) {
        xTaskCreate(&sync_task, "smoke_x_sync_task", 3072, NULL, 5, &xSyncTask);
        return ESP_OK;
    }
    ESP_LOGI(TAG, "smoke_x_sync_task already started");
    return ESP_FAIL;
}

esp_err_t stop_sync() {
    if (xSyncTask) {
        ESP_LOGI(TAG, "Stopping smoke_x_sync_task");
        vTaskDelete(xSyncTask);
        xSyncTask = NULL;
    }
    return ESP_OK;
}

esp_err_t smoke_x_init() {
#if APP_DEBUG > 0
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif

    esp_err_t err = read_config_from_nvram();
    if (!err) {
        err = app_lora_init();
        if (!err && configured) {
            err = set_frequency(config.frequency);
        } else if (!err) {
            start_sync();
        }
    }
    return err;
}

bool smoke_x_is_configured() { return configured; }

esp_err_t smoke_x_sync() {
    strncpy(config.device_id, "", SMOKE_X_DEVICE_ID_LEN);
    config.frequency = 0;
    save_config_to_nvram();
    configured = false;
    sync_received = false;
    esp_err_t err = start_sync();
    return err;
}

esp_err_t smoke_x_get_config(smoke_x_config_t *p_config) {
    memcpy(p_config, &config, sizeof(smoke_x_config_t));
    return ESP_OK;
}

esp_err_t smoke_x_get_state(smoke_x_state_t *p_state) {
    memcpy(p_state, &state, sizeof(smoke_x_state_t));
    return ESP_OK;
}

char *smoke_x_get_data_json() {
    cJSON_ReplaceItemInObject(root, SMOKE_X_BILLOWS,
                              cJSON_CreateBool(state.billows_attached));
    for (unsigned int i = 0; i < config.num_probes; i++) {
        cJSON_ReplaceItemInObject(probes[i], SMOKE_X_CURRENT_TEMP,
                                  cJSON_CreateNumber(state.probes[i].temp));
        cJSON_ReplaceItemInObject(probes[i], SMOKE_X_ALARM_MAX,
                                  cJSON_CreateNumber(state.probes[i].max_temp));
        cJSON_ReplaceItemInObject(probes[i], SMOKE_X_ALARM_MIN,
                                  cJSON_CreateNumber(state.probes[i].min_temp));
    }
    bool success =
        cJSON_PrintPreallocated(root, json_str, sizeof(json_str), false);
    if (success) {
        return json_str;
    }
    return NULL;
}

unsigned int smoke_x_get_num_records() {
    return cJSON_GetArraySize(probes_history[0]);
}

char *smoke_x_get_device_id() { return config.device_id; }

char *smoke_x_get_units() { return state.units; }

esp_err_t smoke_x_start() {
    app_lora_start_rx(handle_rx);
    return ESP_OK;
}

esp_err_t smoke_x_stop() {
    app_lora_stop_rx();
    return ESP_OK;
}
