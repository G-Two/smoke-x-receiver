#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_event.h>
#include <esp_log.h>
#include <string.h>
#include <esp_wpa2.h>
#include <esp_netif.h>
#include <nvs.h>
#include "app_wifi.h"

#define STA_CONNECT_TIMEOUT_MS 30000
#define HOSTNAME "smoke_x"

static const char *TAG = "app_wifi";
static EventGroupHandle_t wifi_event_group = NULL;
static esp_netif_t *sta_netif = NULL;
static const int CONNECTED_BIT = BIT0;
static app_wifi_params_t app_wifi_params;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    // STA
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }

    // AP
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event =
            (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac),
                 event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event =
            (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac),
                 event->aid);
    }
}

static void app_wifi_init_ap(const char *ssid, const char *password) {
    sta_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {.ap = {.ssid = {0},
                                        .ssid_len = strlen(ssid),
                                        .channel = 1,
                                        .password = {0},
                                        .authmode = WIFI_AUTH_OPEN,
                                        .max_connection = 4}};
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));

    if (password) {
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char *)wifi_config.ap.password, password,
                sizeof(wifi_config.ap.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, HOSTNAME));

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password,
             wifi_config.ap.channel);
}

static void app_wifi_init_sta(const char *ssid, const char *password) {
    if (!wifi_event_group) {
        wifi_event_group = xEventGroupCreate();
    }

    sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, MAX_SSID_LEN);
    ESP_LOGI(TAG, "Wifi config SSID: %s", wifi_config.sta.ssid);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, MAX_PASSPHRASE_LEN);
        ESP_LOGI(TAG, "Wifi config password: %s", wifi_config.sta.password);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}

bool app_wifi_validate_params(app_wifi_params_t *params) {
    bool valid = false;

    if (params->mode == WIFI_MODE_AP) {
        if (params->auth_type == WIFI_AUTH_WPA2_PSK) {
            if (strlen(params->ssid) > 1 && strlen(params->password) > 8)
                valid = true;
        } else if (params->auth_type == WIFI_AUTH_OPEN) {
            if (strlen(params->ssid) > 1) valid = true;
        }
    } else if (params->mode == WIFI_MODE_STA) {
        if (params->auth_type == WIFI_AUTH_WPA2_PSK) {
            if (strlen(params->ssid) > 1 && strlen(params->password) > 1)
                valid = true;
        } else if (params->auth_type == WIFI_AUTH_WPA2_ENTERPRISE) {
            if (strlen(params->ssid) > 1 && strlen(params->password) > 1 &&
                strlen(params->username) > 1)
                valid = true;
        }
    }

    params->valid = valid;
    return valid;
}

void app_wifi_get_params(app_wifi_params_t *params) {
    memcpy(params, &app_wifi_params, sizeof(app_wifi_params_t));
}

void app_wifi_set_params(app_wifi_params_t *params) {
    nvs_handle_t h_nvs;

    nvs_open("wifi_config", NVS_READWRITE, &h_nvs);
    nvs_set_blob(h_nvs, "config", params, sizeof(app_wifi_params_t));
    nvs_close(h_nvs);

    esp_wifi_stop();
    esp_restart();  // There's probably a more graceful way of doing this
}

static void sta_fail_detect(void *params) {
    TickType_t start_tick = xTaskGetTickCount();

    while (1) {
        if (pdTICKS_TO_MS(xTaskGetTickCount() - start_tick) >
            STA_CONNECT_TIMEOUT_MS) {
            ESP_LOGE(
                TAG,
                "Failed to connect to Wi-Fi AP, reverting to default AP mode");
            esp_wifi_stop();
            app_wifi_init_ap(CONFIG_DEFAULT_WIFI_AP_SSID,
                             CONFIG_DEFAULT_WIFI_AP_PASSWORD);
            break;
        } else if (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    vTaskDelete(h);
}

void app_wifi_init() {
    esp_err_t err;
    nvs_handle_t h_nvs;
    size_t len = 0;

    err = nvs_open("wifi_config", NVS_READWRITE, &h_nvs);
    err = nvs_get_blob(h_nvs, "config", NULL, &len);
    err = nvs_get_blob(h_nvs, "config", &app_wifi_params, &len);
    nvs_close(h_nvs);

    if (!err && app_wifi_validate_params(&app_wifi_params)) {
        ESP_LOGD(
            TAG,
            "mode: %d, auth_type: %d, ssid: %s, username: %s, password: %s",
            app_wifi_params.mode, app_wifi_params.auth_type,
            app_wifi_params.ssid, app_wifi_params.username,
            app_wifi_params.password);

        if (app_wifi_params.mode == WIFI_MODE_STA) {
            switch (app_wifi_params.auth_type) {
                case WIFI_AUTH_WPA2_PSK:
                    app_wifi_init_sta(app_wifi_params.ssid,
                                      app_wifi_params.password);
                    break;
                case WIFI_AUTH_WPA2_ENTERPRISE:
                    app_wifi_init_sta(app_wifi_params.ssid, NULL);
                    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_identity(
                        (unsigned char *)app_wifi_params.username,
                        strlen(app_wifi_params.username)));
                    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_username(
                        (unsigned char *)app_wifi_params.username,
                        strlen(app_wifi_params.username)));
                    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_password(
                        (unsigned char *)app_wifi_params.password,
                        strlen(app_wifi_params.password)));
                    ESP_ERROR_CHECK(
                        esp_wifi_sta_wpa2_ent_set_ttls_phase2_method(
                            ESP_EAP_TTLS_PHASE2_MSCHAPV2));
                    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_enable());
                    break;
                case WIFI_AUTH_OPEN:
                    app_wifi_init_sta(app_wifi_params.ssid, NULL);
                    break;
                default:
                    ESP_LOGE(TAG, "Unsupported wifi auth mode");
            }
            ESP_ERROR_CHECK(esp_wifi_start());
            ESP_ERROR_CHECK(
                tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, HOSTNAME));

            xTaskCreate(&sta_fail_detect, "app_wifi_sta_fail_detect", 4096,
                        NULL, 5, NULL);
        } else if (app_wifi_params.mode == WIFI_MODE_AP) {
            app_wifi_init_ap(app_wifi_params.ssid, app_wifi_params.password);
        }

    } else {
        ESP_LOGI(
            TAG,
            "No valid wifi configuration found, starting in default AP mode");
        app_wifi_init_ap(CONFIG_DEFAULT_WIFI_AP_SSID,
                         CONFIG_DEFAULT_WIFI_AP_PASSWORD);
    }
}
