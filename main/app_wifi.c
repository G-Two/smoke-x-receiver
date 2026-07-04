#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <esp_eap_client.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <nvs.h>
#include "app_wifi.h"

#define STA_CONNECT_TIMEOUT_MS 30000
#define HOSTNAME "smokex"
/* Brief pause before reconfiguring Wi-Fi from app_wifi_set_params so the
 * HTTP response to the form submission has time to flush over the
 * about-to-be-torn-down AP. */
#define SET_PARAMS_FLUSH_DELAY_MS 500

static const char *TAG = "app_wifi";
static EventGroupHandle_t wifi_event_group = NULL;
static esp_netif_t *sta_netif = NULL;
static const int CONNECTED_BIT = BIT0;
static app_wifi_params_t app_wifi_params;
static bool wifi_inited = false;
static TaskHandle_t sta_fail_detect_task = NULL;

static void apply_params(const app_wifi_params_t *params);

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    // STA
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event =
            (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "STA disconnected, reason: %d", event->reason);
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

/* Stop the Wi-Fi stack and tear down whatever default netif is currently
 * active. Safe to call when nothing is running. Keeps the event group and
 * event handler registrations in place so they apply to whatever netif
 * we create next. */
static void teardown_current_netif(void) {
    esp_wifi_stop();
    if (sta_netif) {
        esp_netif_destroy_default_wifi(sta_netif);
        sta_netif = NULL;
    }
}

/* One-time bring-up of the underlying Wi-Fi stack and event handlers.
 * Per-mode netif / config setup is done by app_wifi_init_{ap,sta}. */
static void ensure_wifi_inited(void) {
    if (wifi_inited) return;
    if (!wifi_event_group) {
        wifi_event_group = xEventGroupCreate();
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));
    wifi_inited = true;
}

static void app_wifi_init_ap(const char *ssid, const char *password) {
    teardown_current_netif();
    ensure_wifi_inited();
    sta_netif = esp_netif_create_default_wifi_ap();

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

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password,
             wifi_config.ap.channel);
}

static void app_wifi_init_sta(const char *ssid, const char *password) {
    teardown_current_netif();
    ensure_wifi_inited();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

    /* Without the esp_restart() of the old reconfigure path, EAP state
     * from a previous WPA2-Enterprise session survives in the driver and
     * breaks PSK/open connects. Clear it; the enterprise branch of
     * apply_params re-enables it right after this returns. */
    esp_wifi_sta_enterprise_disable();

    sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, MAX_SSID_LEN);
    ESP_LOGI(TAG, "Wifi config SSID: %s", wifi_config.sta.ssid);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, MAX_PASSPHRASE_LEN);
        ESP_LOGI(TAG, "Wifi config password: %s", wifi_config.sta.password);
    }
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = true;

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

static void sta_fail_detect(void *arg) {
    (void)arg;
    TickType_t start_tick = xTaskGetTickCount();

    while (1) {
        if (pdTICKS_TO_MS(xTaskGetTickCount() - start_tick) >
            STA_CONNECT_TIMEOUT_MS) {
            ESP_LOGE(
                TAG,
                "Failed to connect to Wi-Fi AP, reverting to default AP mode");
            app_wifi_init_ap(CONFIG_DEFAULT_WIFI_AP_SSID,
                             CONFIG_DEFAULT_WIFI_AP_PASSWORD);
            break;
        } else if (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    sta_fail_detect_task = NULL;
    vTaskDelete(NULL);
}

/* If a previous reconfigure left an sta_fail_detect task running, kill it
 * so we can start a fresh one matching the new connection attempt. */
static void cancel_sta_fail_detect(void) {
    if (sta_fail_detect_task) {
        vTaskDelete(sta_fail_detect_task);
        sta_fail_detect_task = NULL;
    }
}

/* Apply a (validated) params blob: bring up STA or AP mode in place,
 * including EAP for WPA2-Enterprise, and (re)start sta_fail_detect for
 * STA modes. Shared between app_wifi_init (boot path) and app_wifi_set_params
 * (runtime reconfigure path). */
static void apply_params(const app_wifi_params_t *params) {
    cancel_sta_fail_detect();

    if (params->mode == WIFI_MODE_STA) {
        switch (params->auth_type) {
            case WIFI_AUTH_WPA2_PSK:
                app_wifi_init_sta(params->ssid, params->password);
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                app_wifi_init_sta(params->ssid, NULL);
                ESP_ERROR_CHECK(esp_eap_client_set_identity(
                    (unsigned char *)params->username,
                    strlen(params->username)));
                ESP_ERROR_CHECK(esp_eap_client_set_username(
                    (unsigned char *)params->username,
                    strlen(params->username)));
                ESP_ERROR_CHECK(esp_eap_client_set_password(
                    (unsigned char *)params->password,
                    strlen(params->password)));
                ESP_ERROR_CHECK(esp_eap_client_set_ttls_phase2_method(
                    ESP_EAP_TTLS_PHASE2_MSCHAPV2));
                ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());
                break;
            case WIFI_AUTH_OPEN:
                app_wifi_init_sta(params->ssid, NULL);
                break;
            default:
                ESP_LOGE(TAG, "Unsupported wifi auth mode");
                return;
        }
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, HOSTNAME));

        if (xTaskCreate(&sta_fail_detect, "app_wifi_sta_fail_detect", 4096, NULL, 5,
                        &sta_fail_detect_task) != pdPASS) {
            sta_fail_detect_task = NULL;
            ESP_LOGE(TAG, "Failed to create STA fail-detect task");
        }
    } else if (params->mode == WIFI_MODE_AP) {
        app_wifi_init_ap(params->ssid, params->password);
    }
}

/* Background task: delay briefly so the HTTP response that called us can
 * flush before we tear down the current netif, then apply the new params.
 * `arg` is a heap-owned copy of app_wifi_params_t — freed here. */
static void set_params_task(void *arg) {
    app_wifi_params_t *params = arg;
    vTaskDelay(pdMS_TO_TICKS(SET_PARAMS_FLUSH_DELAY_MS));
    apply_params(params);
    free(params);
    vTaskDelete(NULL);
}

void app_wifi_set_params(app_wifi_params_t *params) {
    nvs_handle_t h_nvs;
    if (nvs_open("wifi_config", NVS_READWRITE, &h_nvs) == ESP_OK) {
        nvs_set_blob(h_nvs, "config", params, sizeof(app_wifi_params_t));
        nvs_commit(h_nvs);
        nvs_close(h_nvs);
    }
    memcpy(&app_wifi_params, params, sizeof(app_wifi_params_t));

    /* Caller (HTTP handler) is running in the httpd task; doing the
     * reconfigure inline would tear down the AP before the response
     * makes it out. Defer to a one-shot task that waits a moment first. */
    app_wifi_params_t *copy = malloc(sizeof(*copy));
    if (!copy) {
        ESP_LOGE(TAG, "Failed to allocate params copy");
        return;
    }
    memcpy(copy, params, sizeof(*copy));
    BaseType_t ok = xTaskCreate(&set_params_task, "app_wifi_set_params", 4096, copy, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to start app_wifi_set_params task");
        free(copy);
    }
}

void app_wifi_init() {
    esp_err_t err;
    nvs_handle_t h_nvs;
    size_t len = 0;

#if APP_DEBUG > 0
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif

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
        apply_params(&app_wifi_params);
    } else {
        ESP_LOGI(
            TAG,
            "No valid wifi configuration found, starting in default AP mode");
        app_wifi_init_ap(CONFIG_DEFAULT_WIFI_AP_SSID,
                         CONFIG_DEFAULT_WIFI_AP_PASSWORD);
    }
}

bool app_wifi_is_connected() {
    if (!wifi_event_group) {
        return false;
    }
    return (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) != 0;
}

bool app_wifi_is_ap_mode() {
    wifi_mode_t mode;

    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return app_wifi_params.mode == WIFI_MODE_AP;
    }

    return mode == WIFI_MODE_AP;
}

esp_err_t app_wifi_get_ip_str(char *buf, size_t len) {
    esp_netif_ip_info_t ip_info;

    if (!buf || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sta_netif) {
        snprintf(buf, len, "-");
        return ESP_OK;
    }

    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK) {
        snprintf(buf, len, "-");
        return ESP_OK;
    }

    snprintf(buf, len, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

esp_err_t app_wifi_try_connect(const char *ssid, const char *password,
                               uint32_t timeout_ms, char *out_ip_str,
                               size_t out_ip_str_len) {
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;

    /* The persisted params struct holds a NUL-terminated SSID in a 32-byte
     * field, so 32-char SSIDs (legal per 802.11 and accepted by the Improv
     * parser) would connect now but be truncated in NVS and fail on the
     * next boot. Reject loudly rather than truncate silently. */
    app_wifi_params_t size_check;
    if (strlen(ssid) >= sizeof(size_check.ssid) ||
        strlen(password) >= sizeof(size_check.password)) {
        ESP_LOGW(TAG, "SSID or password too long to persist (max %u/%u chars)",
                 (unsigned)sizeof(size_check.ssid) - 1,
                 (unsigned)sizeof(size_check.password) - 1);
        return ESP_ERR_INVALID_ARG;
    }

    /* A watchdog from a previous STA attempt (e.g. boot with stale
     * credentials) would see CONNECTED_BIT cleared below and yank the
     * netif out from under us mid-connect. Kill it first, the same way
     * apply_params does. */
    cancel_sta_fail_detect();

    /* Tear down whatever Wi-Fi mode is currently active so we can re-init
     * cleanly as STA. esp_wifi_stop() is idempotent; the netif may not yet
     * exist if init was never called. */
    esp_wifi_stop();

    if (sta_netif) {
        esp_netif_destroy_default_wifi(sta_netif);
        sta_netif = NULL;
    }

    if (!wifi_event_group) {
        wifi_event_group = xEventGroupCreate();
    }
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

    sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif) return ESP_FAIL;

    /* Wi-Fi driver and event handlers are initialized once in ensure_wifi_inited(). */
    ensure_wifi_inited();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    /* Improv provisions PSK networks only; clear any EAP state left over
     * from a previous WPA2-Enterprise configuration. */
    esp_wifi_sta_enterprise_disable();

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, MAX_SSID_LEN);
    strncpy((char *)wifi_config.sta.password, password, MAX_PASSPHRASE_LEN);
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = true;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* WIFI_EVENT_STA_START handler triggers esp_wifi_connect(); no need
     * to call it here. */

    EventBits_t bits =
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE, pdTRUE,
                            pdMS_TO_TICKS(timeout_ms));

    if (!(bits & CONNECTED_BIT)) {
        ESP_LOGW(TAG, "Improv hot-connect to '%s' timed out after %lu ms", ssid,
                 (unsigned long)timeout_ms);
        /* We tore down whatever mode was active (usually the config AP)
         * to make this attempt. Don't strand the device retrying bad
         * credentials with no network presence — restore the previous
         * configuration, or the default AP if none exists. */
        if (app_wifi_params.valid) {
            apply_params(&app_wifi_params);
        } else {
            app_wifi_init_ap(CONFIG_DEFAULT_WIFI_AP_SSID,
                             CONFIG_DEFAULT_WIFI_AP_PASSWORD);
        }
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, HOSTNAME));

    if (out_ip_str && out_ip_str_len > 0) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
            snprintf(out_ip_str, out_ip_str_len, IPSTR, IP2STR(&ip_info.ip));
        } else {
            snprintf(out_ip_str, out_ip_str_len, "-");
        }
    }

    /* Persist credentials so the next boot uses them. */
    app_wifi_params_t params = {
        .mode = WIFI_MODE_STA,
        .auth_type = WIFI_AUTH_WPA2_PSK,
        .valid = true,
    };
    strncpy(params.ssid, ssid, sizeof(params.ssid) - 1);
    strncpy(params.password, password, sizeof(params.password) - 1);

    nvs_handle_t h_nvs;
    if (nvs_open("wifi_config", NVS_READWRITE, &h_nvs) == ESP_OK) {
        nvs_set_blob(h_nvs, "config", &params, sizeof(params));
        nvs_commit(h_nvs);
        nvs_close(h_nvs);
        memcpy(&app_wifi_params, &params, sizeof(params));
    }

    ESP_LOGI(TAG, "Improv hot-connect to '%s' succeeded (%s)", ssid,
             out_ip_str ? out_ip_str : "");
    return ESP_OK;
}
