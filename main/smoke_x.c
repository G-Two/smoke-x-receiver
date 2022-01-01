#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs.h>
#include "app_lora.h"
#include "smoke_x.h"

#define SMOKE_X_SYNC_FREQ 920000000
#define SMOKE_X_RF_MIN 902000000
#define SMOKE_X_RF_MAX 928000000
#define SMOKE_X_NVS_NAMESPACE "smoke_x"
#define SMOKE_X_NVS_CONFIG "config"
#define NUM_COMMAS_SYNC_MSG 6
#define NUM_COMMAS_SUCCESS_MSG 2
#define NUM_COMMAS_X2_STATE_MSG 16
#define NUM_COMMAS_X4_STATE_MSG 26

static const char *TAG = "smoke_x";
static smoke_x_config_t config;
static smoke_x_state_t state;
static bool configured = false;
static TickType_t last_heard = 0;

ESP_EVENT_DEFINE_BASE(SMOKE_X_EVENT);

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
    ESP_LOGI(TAG, "Received sync message: %s", msg);
    char *tmp = strdup(msg);
    strtok(tmp, ",");
    strncpy(config.device_id, strtok(NULL, ","), sizeof(config.device_id));
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
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Sending sync acknowledgement to transmitter");
        app_lora_start_tx(&tx_msg);
    } else {
        ESP_LOGE(TAG, "Frequency out of range %d", config.frequency);
        config.frequency = 0;
    }
}

static void parse_x2_msg(const char *msg, smoke_x_state_t *state) {
    last_heard = xTaskGetTickCount();
    char *tmp = strdup(msg);
    strtok(tmp, ",");  // Not using device ID
    state->unk_1 = atoi(strtok(NULL, ","));
    state->unk_2 = atoi(strtok(NULL, ","));
    state->unk_3 = atoi(strtok(NULL, ","));
    state->probe_1_attached = atoi(strtok(NULL, ",")) == 3 ? false : true;
    state->probe_1_temp = atof(strtok(NULL, ",")) / 10.0;
    state->probe_1_alarm = atoi(strtok(NULL, ","));
    state->probe_1_max = atoi(strtok(NULL, ","));
    state->probe_1_min = atoi(strtok(NULL, ","));
    state->probe_2_attached = atoi(strtok(NULL, ",")) == 3 ? false : true;
    state->probe_2_temp = atof(strtok(NULL, ",")) / 10.0;
    state->probe_2_alarm = atoi(strtok(NULL, ","));
    state->probe_2_max = atoi(strtok(NULL, ","));
    state->probe_2_min = atoi(strtok(NULL, ","));
    state->billows_attached = atoi(strtok(NULL, ","));
    state->unk_4 = atoi(strtok(NULL, ","));
    free(tmp);
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
            if (config.frequency >= SMOKE_X_RF_MIN &&
                config.frequency <= SMOKE_X_RF_MAX) {
                ESP_LOGI(TAG, "Device is paired to %s at %d MHz",
                         config.device_id, config.frequency);
                configured = true;
                return ESP_OK;
            }
        }

        ESP_LOGI(TAG, "Device is not paired, waiting for sync on %d",
                 config.frequency);
        configured = false;
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void handle_rx(const char *msg, const int len) {
    switch (count_commas(msg, len)) {
        case NUM_COMMAS_SYNC_MSG:
            if (!configured) {
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
            if (!configured) {
                configured = true;
                save_config_to_nvram();
                esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_SYNC_SUCCESS, NULL,
                               0, 1000);
                ESP_LOGI(TAG,
                         "Received data transmission from %s, saving config",
                         config.device_id);
            }
            parse_x2_msg(msg, &state);
            ESP_LOGI(TAG, "X2 DATA: %s", msg);
            esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_STATE_X2, NULL, 0,
                           1000);
            break;
        case NUM_COMMAS_X4_STATE_MSG:
            if (!configured) {
                configured = true;
                save_config_to_nvram();
                esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_SYNC_SUCCESS, NULL,
                               0, 1000);
                ESP_LOGI(TAG,
                         "Received data transmission from %s, saving config",
                         config.device_id);
            }
            esp_event_post(SMOKE_X_EVENT, SMOKE_X_EVENT_STATE_X4, NULL, 0,
                           1000);
            break;
        default:
            ESP_LOGE(TAG, "Received unrecognized message type: %s", msg);
            break;
    }
}

esp_err_t smoke_x_init() {
    esp_err_t err = read_config_from_nvram();
    if (!err) {
        err = app_lora_init();
        if (!err && configured) {
            err = set_frequency(config.frequency);
        } else if (!err) {
            err = set_frequency(SMOKE_X_SYNC_FREQ);
        }
    }
    return err;
}

bool smoke_x_is_configured() { return configured; }

esp_err_t smoke_x_sync() {
    strncpy(config.device_id, "", sizeof(config.device_id));
    config.frequency = 0;
    save_config_to_nvram();
    configured = false;
    esp_err_t err = set_frequency(SMOKE_X_SYNC_FREQ);
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

esp_err_t smoke_x_start() {
    app_lora_start_rx(handle_rx);
    return ESP_OK;
}

esp_err_t smoke_x_stop() {
    app_lora_stop_rx();
    return ESP_OK;
}
