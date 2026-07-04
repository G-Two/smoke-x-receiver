#include "app_improv.h"

#include <stdio.h>
#include <string.h>

#include <driver/uart.h>
#include <esp_app_desc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app_wifi.h"
#include "improv.h"

static const char *TAG = "app_improv";

/* The console UART is whatever ESP-IDF's logging uses. On most ESP32
 * targets this is UART0; on the ESP32-S3 it may be the native USB-Serial-
 * JTAG peripheral instead, which is its own device. CONFIG_ESP_CONSOLE_*
 * tells us which. We bind to UART0 by default; this is what ESP Web Tools
 * connects to via the USB-UART bridge on Heltec boards. */
#define IMPROV_UART_NUM UART_NUM_0
#define IMPROV_RX_BUF_SIZE 256
#define IMPROV_CONNECT_TIMEOUT_MS 30000

/* Improv state held by the task. The protocol's state machine is small:
 * AUTHORIZED is the resting state (skipping the optional authorize step),
 * PROVISIONING while we attempt a connection, PROVISIONED on success. */
static uint8_t s_state = IMPROV_STATE_AUTHORIZED;

/* Send a single Improv frame to the host. uart_write_bytes is internally
 * mutex-protected so back-to-back calls from this task won't fragment a
 * frame mid-stream. ESP_LOG output uses the ROM UART path rather than the
 * installed driver, so a log line can still land between our bytes and
 * corrupt the checksum; ESP Web Tools will treat that as a bad frame and
 * the user retries. Acceptable trade-off — using vTaskSuspendAll here
 * would assert when uart_wait_tx_done tries to take its TX-done
 * semaphore with the scheduler suspended. */
static void improv_send(const uint8_t *buf, size_t len) {
    if (!buf || len == 0) return;
    uart_write_bytes(IMPROV_UART_NUM, (const char *)buf, len);
}

static void send_state(uint8_t state) {
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t n = improv_build_state(frame, sizeof(frame), state);
    improv_send(frame, n);
    s_state = state;
}

static void send_error(uint8_t err) {
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t n = improv_build_error(frame, sizeof(frame), err);
    improv_send(frame, n);
}

static void send_rpc_result_url(uint8_t cmd, const char *url) {
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t n = improv_build_rpc_result_string(frame, sizeof(frame), cmd, url);
    improv_send(frame, n);
}

static void send_device_info(void) {
    /* Improv wants four strings: firmware name, version, hardware
     * variant, device name. We pack them into a single result frame with
     * a list-of-strings payload (each prefixed by its length byte). */
    const esp_app_desc_t *app = esp_app_get_description();
#if CONFIG_IDF_TARGET_ESP32S3
    const char *chip = "ESP32-S3";
#else
    const char *chip = "ESP32";
#endif
    const char *fields[4] = {"Smoke X Receiver", app->version, chip,
                             "Smoke X Receiver"};

    /* Build payload manually: [cmd][inner_len][s1_len][s1][s2_len][s2]... */
    uint8_t inner[256];
    inner[0] = IMPROV_CMD_GET_DEVICE_INFO;
    size_t pos = 2; /* reserve inner[1] for inner length */
    for (int i = 0; i < 4; i++) {
        size_t slen = strlen(fields[i]);
        if (pos + 1 + slen > sizeof(inner)) return;
        inner[pos++] = (uint8_t)slen;
        memcpy(&inner[pos], fields[i], slen);
        pos += slen;
    }
    inner[1] = (uint8_t)(pos - 2);

    /* Hand-build a frame around the inner payload. */
    uint8_t frame[IMPROV_FRAME_MAX];
    if (sizeof(frame) < 6 + 1 + 1 + 1 + pos + 1) return;
    memcpy(frame, "IMPROV", 6);
    frame[6] = 1;
    frame[7] = IMPROV_TYPE_RPC_RESULT;
    frame[8] = (uint8_t)pos;
    memcpy(&frame[9], inner, pos);
    uint32_t cs = 0;
    for (size_t i = 0; i < 9 + pos; i++) cs += frame[i];
    frame[9 + pos] = (uint8_t)(cs & 0xff);
    improv_send(frame, 9 + pos + 1);
}

static void handle_wifi_settings(const improv_rpc_t *rpc) {
    ESP_LOGI(TAG, "Improv WIFI_SETTINGS received for SSID '%s'", rpc->ssid);
    send_state(IMPROV_STATE_PROVISIONING);

    char ip_str[16] = {0};
    esp_err_t err =
        app_wifi_try_connect(rpc->ssid, rpc->password,
                             IMPROV_CONNECT_TIMEOUT_MS, ip_str, sizeof(ip_str));
    if (err == ESP_OK) {
        char url[40];
        snprintf(url, sizeof(url), "http://%s/", ip_str);
        send_rpc_result_url(IMPROV_CMD_WIFI_SETTINGS, url);
        send_state(IMPROV_STATE_PROVISIONED);
        /* PROVISIONED is only emitted as an immediate post-success signal.
         * Reset our stored state to AUTHORIZED so a later dialog reopen doesn't
         * see a stale PROVISIONED without the accompanying URL. */
        s_state = IMPROV_STATE_AUTHORIZED;
    } else {
        send_error(IMPROV_ERR_UNABLE_TO_CONNECT);
        send_state(IMPROV_STATE_AUTHORIZED);
    }
}

static void handle_rpc(const improv_rpc_t *rpc) {
    ESP_LOGI(TAG, "Improv RPC received: cmd=0x%02x", rpc->cmd);
    switch (rpc->cmd) {
        case IMPROV_CMD_WIFI_SETTINGS:
            handle_wifi_settings(rpc);
            break;
        case IMPROV_CMD_GET_CURRENT_STATE:
            /* Also covers IDENTIFY (0x02) — ESP Web Tools sends this as
             * its initial probe. We have no LED/buzzer to identify with,
             * so the only useful action is to report state. */
            send_state(s_state);
            break;
        case IMPROV_CMD_GET_DEVICE_INFO:
            send_device_info();
            break;
        case IMPROV_CMD_GET_WIFI_NETWORKS:
            /* Scanning is optional; ESP Web Tools falls back to a manual
             * SSID text input if we don't implement it. */
            send_error(IMPROV_ERR_UNKNOWN_RPC);
            break;
        default:
            send_error(IMPROV_ERR_UNKNOWN_RPC);
            break;
    }
}

static void improv_task(void *arg) {
    (void)arg;

    /* Install the UART driver. The boot ROM / ESP_LOG already use UART0
     * for transmit; uart_driver_install gives us reliable rx buffering
     * and lets uart_write_bytes coexist. */
    const uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(IMPROV_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_driver_install(IMPROV_UART_NUM, IMPROV_RX_BUF_SIZE * 2, 0, 0, NULL, 0));

    improv_parser_t parser;
    improv_parser_reset(&parser);

    uint8_t byte;
    improv_rpc_t rpc;
    ESP_LOGI(TAG, "Improv-Serial listener ready on UART%d", IMPROV_UART_NUM);

    /* No proactive state announce here. ESP Web Tools sends GET_CURRENT_STATE
     * when it opens the serial port, and we respond from handle_rpc. Sending
     * a frame at task startup races with boot-time ESP_LOG output on the
     * same UART and corrupts the frame's checksum on the host side. */

    for (;;) {
        int n = uart_read_bytes(IMPROV_UART_NUM, &byte, 1, pdMS_TO_TICKS(1000));
        if (n == 1) {
            if (improv_parse_byte(&parser, byte, &rpc)) {
                handle_rpc(&rpc);
            }
        }
    }
}

esp_err_t app_improv_start(void) {
    BaseType_t ok = xTaskCreate(improv_task, "app_improv", 4096, NULL, 5, NULL);
    return ok == pdPASS ? ESP_OK : ESP_FAIL;
}
