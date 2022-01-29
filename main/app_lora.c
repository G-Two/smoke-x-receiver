#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "lora.h"
#include "app_lora.h"

static const char *TAG = "app_lora";
static TaskHandle_t xRxTask = NULL;
static TaskHandle_t xTxTask = NULL;
static SemaphoreHandle_t xRadioSemaphore = NULL;

static app_lora_params_t radio_params = {.tx_power = DEFAULT_TX_POWER,
                                         .frequency = DEFAULT_FREQ,
                                         .bandwidth = DEFAULT_BW,
                                         .spreading_factor = DEFAULT_SF,
                                         .preamble_len = DEFAULT_PREAMBLE_LEN,
                                         .sync_word = DEFAULT_SYNC_WORD,
                                         .implicit_hdr = false,
                                         .coding_rate = DEFAULT_CR,
                                         .msg_len = DEFAULT_MSG_LEN,
                                         .crc_on = true};

static bool range_check(int val, int min, int max) {
    return (min <= val && val <= max);
}

static int validate_params(app_lora_params_t *in_params) {
    if (in_params->tx_power) {
        if (range_check(in_params->tx_power, TX_POWER_MIN, TX_POWER_MAX)) {
            radio_params.tx_power = in_params->tx_power;
        } else {
            ESP_LOGE(TAG, "invalid tx power %d", in_params->tx_power);
        }
    }

    if (in_params->frequency) {
        if (range_check(in_params->frequency, FREQ_MIN, FREQ_MAX)) {
            radio_params.frequency = in_params->frequency;
        } else {
            ESP_LOGE(TAG, "invalid frequency %d", in_params->frequency);
        }
    }

    if (in_params->bandwidth) {
        if (range_check(in_params->bandwidth, BW_MIN, BW_MAX)) {
            radio_params.bandwidth = in_params->bandwidth;
        } else {
            ESP_LOGE(TAG, "invalid bandwidth %d", in_params->bandwidth);
        }
    }

    if (in_params->spreading_factor) {
        if (range_check(in_params->spreading_factor, SF_MIN, SF_MAX)) {
            radio_params.spreading_factor = in_params->spreading_factor;
        } else {
            ESP_LOGE(TAG, "invalid spreading_factor %d",
                     in_params->spreading_factor);
        }
    }

    if (in_params->preamble_len) {
        if (range_check(in_params->preamble_len, PREAMBLE_MIN, PREAMBLE_MAX)) {
            radio_params.preamble_len = in_params->preamble_len;
        } else {
            ESP_LOGE(TAG, "invalid preamble length %d",
                     in_params->preamble_len);
        }
    }

    if (in_params->sync_word) {
        if (range_check(in_params->sync_word, SYNC_WORD_MIN, SYNC_WORD_MAX)) {
            radio_params.sync_word = in_params->sync_word;
        } else {
            ESP_LOGE(TAG, "invalid sync word %d", in_params->sync_word);
        }
    }

    if (in_params->coding_rate) {
        if (range_check(in_params->coding_rate, CR_MIN, CR_MAX)) {
            radio_params.coding_rate = in_params->coding_rate;
        } else {
            ESP_LOGE(TAG, "invalid coding rate %d", in_params->coding_rate);
        }
    }

    return ESP_OK;
}

static void tx_msg(char *msg) {
    size_t len = strlen(msg);
    xSemaphoreTake(xRadioSemaphore, (TickType_t)10);
    lora_send_packet((uint8_t *)msg, len);
    ESP_LOGI(TAG, "%d bytes transmitted (%s)", len, msg);
    if (xRxTask) {
        lora_receive();
    }
    xSemaphoreGive(xRadioSemaphore);
}

static void tx_task(void *pvParameter) {
    if (pvParameter) {
        app_lora_tx_msg_t *args = (app_lora_tx_msg_t *)pvParameter;
        char msg[PAYLOAD_LEN_MAX];
        strncpy(msg, args->msg, PAYLOAD_LEN_MAX);
        xTaskNotifyGive(args->sending_task);
        uint32_t interval = args->repeat_interval_ms;

        ESP_LOGI(TAG, "Starting LoRa Tx msg='%s' interval=%dms", msg, interval);
        while (1) {
            tx_msg(msg);
            vTaskDelay(pdMS_TO_TICKS(interval));
        }
    }
}

static void rx_task(void *pvParameter) {
    int msg_len;
    int rssi;
    float snr;
    uint8_t buf[255];
    void (*cb)(const char *, const int) = pvParameter;
    ESP_LOGI(TAG, "Starting LoRa Rx");
    while (1) {
        xSemaphoreTake(xRadioSemaphore, (TickType_t)10);
        lora_receive();
        while (lora_received()) {
            msg_len = lora_receive_packet(buf, sizeof(buf));
            rssi = lora_packet_rssi();
            snr = lora_packet_snr();
            buf[msg_len] = 0;
            ESP_LOGD(TAG, "Packet received - Size: %d RSSI: %d, SNR: %f",
                     msg_len, rssi, snr);
            ESP_LOGD(TAG, "%s", buf);
            cb((char *)buf, msg_len);
            lora_receive();
        }
        xSemaphoreGive(xRadioSemaphore);
        vTaskDelay(1);
    }
}

int app_lora_start_tx(app_lora_tx_msg_t *args) {
    if (args->repeat_interval_ms > 0) {
        if (!xTxTask) {
            xTaskCreate(&tx_task, "app_lora_tx_task", 3072, args, 5, &xTxTask);
            return ESP_OK;
        }
        ESP_LOGI(TAG, "app_lora_tx_task already started");
        return ESP_FAIL;
    }
    // Transmit one message only, don't start new task
    tx_msg(args->msg);
    xTaskNotifyGive(args->sending_task);
    return ESP_OK;
}

int app_lora_start_rx(void (*cb)(const char *, const int)) {
    if (!xRxTask) {
        xTaskCreate(&rx_task, "app_lora_rx_task", 3072, cb, 5, &xRxTask);
        return ESP_OK;
    }
    ESP_LOGI(TAG, "app_lora_rx_task already started");
    return ESP_FAIL;
}

int app_lora_stop_tx() {
    if (xTxTask) {
        ESP_LOGI(TAG, "Stopping LoRa Tx");
        vTaskDelete(xTxTask);
        xTxTask = NULL;
    }
    return ESP_OK;
}

int app_lora_stop_rx() {
    if (xRxTask) {
        ESP_LOGI(TAG, "Stopping LoRa Rx");
        vTaskDelete(xRxTask);
        xRxTask = NULL;
    }
    return ESP_OK;
}

int app_lora_get_params(app_lora_params_t *out_params) {
    if (out_params) {
        memcpy(out_params, &radio_params, sizeof(app_lora_params_t));
        return ESP_OK;
    }
    return ESP_FAIL;
}

int app_lora_set_params(app_lora_params_t *in_params,
                        xTaskHandle calling_task) {
    if (validate_params(in_params) == ESP_OK) {
        ESP_LOGI(TAG, "Setting radio parameters");
        xSemaphoreTake(xRadioSemaphore, (TickType_t)10);
        lora_idle();
        ESP_LOGI(TAG, "  Frequency %d", radio_params.frequency);
        lora_set_frequency(radio_params.frequency);
        ESP_LOGI(TAG, "  Bandwidth %d", radio_params.bandwidth);
        lora_set_bandwidth(radio_params.bandwidth);
        ESP_LOGI(TAG, "  Spreading Factor %d", radio_params.spreading_factor);
        lora_set_spreading_factor(radio_params.spreading_factor);
        ESP_LOGI(TAG, "  Transmit Power %d", radio_params.tx_power);
        lora_set_tx_power(radio_params.tx_power);
        ESP_LOGI(TAG, "  Coding Rate %d", radio_params.coding_rate);
        lora_set_coding_rate(radio_params.coding_rate);
        ESP_LOGI(TAG, "  Sync Word %d", radio_params.sync_word);
        lora_set_sync_word(radio_params.sync_word);
        ESP_LOGI(TAG, "  Implicit Header %d", radio_params.implicit_hdr);
        if (radio_params.implicit_hdr) {
            lora_explicit_header_mode();
        } else {
            // lora_implicit_header_mode(radio_params.payload_len);
        }
        ESP_LOGI(TAG, "  CRC Enable %d", radio_params.crc_on);
        if (radio_params.crc_on)
            lora_enable_crc();
        else {
            lora_disable_crc();
        }
        xSemaphoreGive(xRadioSemaphore);

        if (calling_task) {
            xTaskNotifyGive(calling_task);
            ESP_LOGI(
                TAG,
                "New radio parameters set\n f=%d\n bw=%d\n sf=%d\n "
                "tx_power=%d\n cr=%d\n sync=%x\n impl_hdr=%d\n crc_on=%d\n",
                radio_params.frequency, radio_params.bandwidth,
                radio_params.spreading_factor, radio_params.tx_power,
                radio_params.coding_rate, radio_params.sync_word,
                radio_params.implicit_hdr, radio_params.crc_on);
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

int app_lora_init() {
    if (lora_init()) {
        xRadioSemaphore = xSemaphoreCreateBinary();
        app_lora_set_params(&radio_params, NULL);
        ESP_LOGI(TAG, "LoRa module initialized");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error initializing LoRa");
        return ESP_FAIL;
    }
}
