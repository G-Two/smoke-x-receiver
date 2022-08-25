#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include "app_mqtt.h"
#include "app_web_ui.h"
#include "app_wifi.h"
#include "smoke_x.h"

static const char* TAG = "smoke_x_main";

#if APP_DEBUG == 2
static char task_list_buf[1600];
void debug_task_list() {
    while (true) {
        vTaskList(task_list_buf);
        printf("\n\nTASK LIST\n%s\n", task_list_buf);
        vTaskDelay(pdMS_TO_TICKS(6000));
    }
}
#endif

void smoke_x_event_handler(void* handler_arg, esp_event_base_t base,
                           int32_t event_id, void* event_data) {
    switch (event_id) {
        case SMOKE_X_EVENT_SYNC:
            // Do something with the web UI?
            break;
        case SMOKE_X_EVENT_SYNC_SUCCESS:
            // Do something with the web UI?
            break;
        case SMOKE_X_EVENT_STATE_MSG_RECEIVED:
            if (app_mqtt_is_connected()) {
                app_mqtt_publish_state();
            }
            break;
        case SMOKE_X_EVENT_DISCOVERY_REQUIRED:
            if (app_mqtt_is_connected()) {
                app_mqtt_publish_discovery();
            }
            break;
        default:
            ESP_LOGE(TAG, "Unknown message type received: %d", event_id);
    }
}

void run_when_disconnected(void* handler_arg, esp_event_base_t base, int32_t id,
                           void* event_data) {
    ESP_LOGI(TAG, "Wi-Fi connection lost");
    if (app_mqtt_is_connected()) {
        app_mqtt_stop();
    }
}

void run_when_ip_addr_obtained(void* handler_arg, esp_event_base_t base,
                               int32_t id, void* event_data) {
    ESP_LOGI(TAG, "IP address obtained");
    app_mqtt_start();
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                               &run_when_disconnected, NULL);
}

void app_main() {
    esp_log_level_set("*", ESP_LOG_INFO);
#if APP_DEBUG > 0
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &run_when_ip_addr_obtained, NULL);
    esp_event_handler_register(SMOKE_X_EVENT, ESP_EVENT_ANY_ID,
                               &smoke_x_event_handler, NULL);

    smoke_x_init();
    smoke_x_start();
    app_wifi_init();
    app_web_ui_start();

#if APP_DEBUG == 2
    xTaskCreate(&debug_task_list, "debug_task_list", 4096, NULL, 5, NULL);
#endif
}
