#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "app_display.h"
#include "app_display_font.h"
#include "app_mqtt.h"
#include "app_wifi.h"
#include "smoke_x.h"

#if CONFIG_HELTEC_OLED

#define SSD1306_ADDR 0x3C
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_PAGES (SSD1306_HEIGHT / 8)
#define FONT_WIDTH 6
#define FONT_HEIGHT 8

static const char *TAG = "app_display";
static uint8_t framebuffer[SSD1306_WIDTH * SSD1306_PAGES];
static bool display_ready = false;

static esp_err_t ssd1306_write_cmd(uint8_t cmd) {
    uint8_t buffer[2] = {0x00, cmd};
    return i2c_master_write_to_device(I2C_NUM_0, SSD1306_ADDR, buffer,
                                      sizeof(buffer), pdMS_TO_TICKS(100));
}

static esp_err_t ssd1306_write_data(const uint8_t *data, size_t len) {
    uint8_t buffer[17];
    size_t offset = 0;

    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > 16) {
            chunk = 16;
        }

        buffer[0] = 0x40;
        memcpy(&buffer[1], data + offset, chunk);
        esp_err_t err = i2c_master_write_to_device(
            I2C_NUM_0, SSD1306_ADDR, buffer, chunk + 1, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            return err;
        }
        offset += chunk;
    }

    return ESP_OK;
}

static esp_err_t ssd1306_init_panel() {
    const uint8_t init_seq[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00,
                                0x40, 0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8,
                                0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB,
                                0x40, 0xA4, 0xA6, 0xAF};

    for (size_t i = 0; i < sizeof(init_seq); i++) {
        ESP_RETURN_ON_ERROR(ssd1306_write_cmd(init_seq[i]), TAG,
                            "init command failed");
    }

    memset(framebuffer, 0, sizeof(framebuffer));
    return ssd1306_write_data(framebuffer, sizeof(framebuffer));
}

static void display_power_on() {
    if (CONFIG_HELTEC_OLED_VEXT_GPIO >= 0) {
        gpio_set_direction(CONFIG_HELTEC_OLED_VEXT_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(CONFIG_HELTEC_OLED_VEXT_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (CONFIG_HELTEC_OLED_RST_GPIO >= 0) {
        gpio_set_direction(CONFIG_HELTEC_OLED_RST_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(CONFIG_HELTEC_OLED_RST_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(CONFIG_HELTEC_OLED_RST_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void framebuffer_clear() { memset(framebuffer, 0, sizeof(framebuffer)); }

static void framebuffer_draw_char(int x, int y, char c) {
    if (c < 0x20 || c > 0x7E) {
        c = '?';
    }

    const uint8_t *glyph = &font5x7[(c - 0x20) * 5];
    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            if ((line >> row) & 0x01) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < SSD1306_WIDTH && py >= 0 &&
                    py < SSD1306_HEIGHT) {
                    framebuffer[px + (py / 8) * SSD1306_WIDTH] |= 1 << (py % 8);
                }
            }
        }
    }
}

static void framebuffer_draw_text(int x, int y, const char *text) {
    while (*text) {
        framebuffer_draw_char(x, y, *text++);
        x += FONT_WIDTH;
        if (x > SSD1306_WIDTH - FONT_WIDTH) {
            break;
        }
    }
}

static esp_err_t framebuffer_flush() {
    for (int page = 0; page < SSD1306_PAGES; page++) {
        ESP_RETURN_ON_ERROR(ssd1306_write_cmd(0xB0 + page), TAG,
                            "set page failed");
        ESP_RETURN_ON_ERROR(ssd1306_write_cmd(0x00), TAG,
                            "set lower col failed");
        ESP_RETURN_ON_ERROR(ssd1306_write_cmd(0x10), TAG,
                            "set upper col failed");
        ESP_RETURN_ON_ERROR(
            ssd1306_write_data(&framebuffer[page * SSD1306_WIDTH],
                               SSD1306_WIDTH),
            TAG, "write page failed");
    }
    return ESP_OK;
}

static void format_uptime_line(char *line, size_t len) {
    uint32_t total_sec = (uint32_t)(esp_timer_get_time() / 1000000);
    uint32_t hours = total_sec / 3600;
    uint32_t mins = (total_sec % 3600) / 60;
    uint32_t secs = total_sec % 60;

    snprintf(line, len, "Up %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, hours,
             mins, secs);
}

static void format_wifi_line(char *line, size_t len) {
    char ip[16];

    app_wifi_get_ip_str(ip, sizeof(ip));
    if (app_wifi_is_ap_mode()) {
        snprintf(line, len, "WiFi AP %s", ip);
    } else if (app_wifi_is_connected()) {
        snprintf(line, len, "WiFi STA %s", ip);
    } else {
        snprintf(line, len, "WiFi STA connecting");
    }
}

static void format_mqtt_line(char *line, size_t len) {
    if (!app_mqtt_is_enabled()) {
        snprintf(line, len, "MQTT disabled");
    } else if (app_mqtt_is_connected()) {
        snprintf(line, len, "MQTT connected");
    } else {
        snprintf(line, len, "MQTT waiting");
    }
}

static void format_lora_line(char *line, size_t len) {
    smoke_x_config_t config;

    smoke_x_get_config(&config);
    if (config.frequency == 0) {
        snprintf(line, len, "LoRa sync pending");
        return;
    }

    if (smoke_x_is_configured()) {
        snprintf(line, len, "LoRa %.1f MHz", config.frequency / 1000000.0);
    } else {
        snprintf(line, len, "LoRa sync %.0f MHz", config.frequency / 1000000.0);
    }
}

static void format_device_line(char *line, size_t len) {
    smoke_x_config_t config;

    smoke_x_get_config(&config);
    if (smoke_x_is_configured() && config.device_id[0] != '\0') {
        snprintf(line, len, "ID %s", config.device_id);
        return;
    }

    snprintf(line, len, "ID unpaired");
}

static void format_probe_line(char *line, size_t len) {
    smoke_x_state_t state;
    smoke_x_config_t config;

    smoke_x_get_state(&state);
    smoke_x_get_config(&config);

    if (!smoke_x_is_configured() || config.num_probes == 0) {
        line[0] = '\0';
        return;
    }

    if (config.num_probes >= 2) {
        snprintf(line, len, "P1:%.0f P2:%.0f", state.probes[0].temp,
                 state.probes[1].temp);
    } else {
        snprintf(line, len, "P1:%.0f", state.probes[0].temp);
    }
}

static void app_display_refresh() {
    char line[32];
    int y = 0;

    if (!display_ready) {
        return;
    }

    framebuffer_clear();
    framebuffer_draw_text(0, y, "Smoke X Rx");
    y += FONT_HEIGHT;

    format_uptime_line(line, sizeof(line));
    framebuffer_draw_text(0, y, line);
    y += FONT_HEIGHT;

    format_wifi_line(line, sizeof(line));
    framebuffer_draw_text(0, y, line);
    y += FONT_HEIGHT;

    format_mqtt_line(line, sizeof(line));
    framebuffer_draw_text(0, y, line);
    y += FONT_HEIGHT;

    format_lora_line(line, sizeof(line));
    framebuffer_draw_text(0, y, line);
    y += FONT_HEIGHT;

    format_device_line(line, sizeof(line));
    framebuffer_draw_text(0, y, line);
    y += FONT_HEIGHT;

    format_probe_line(line, sizeof(line));
    if (line[0] != '\0') {
        framebuffer_draw_text(0, y, line);
    }

    esp_err_t err = framebuffer_flush();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "flush failed: %s", esp_err_to_name(err));
    }
}

static void display_task(void *arg) {
    while (true) {
        app_display_refresh();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t app_display_init() {
    esp_err_t err;

    display_power_on();

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_HELTEC_OLED_SDA_GPIO,
        .scl_io_num = CONFIG_HELTEC_OLED_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_HELTEC_OLED_I2C_FREQ_HZ,
    };

    err = i2c_param_config(I2C_NUM_0, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C install failed: %s", esp_err_to_name(err));
        return err;
    }

    err = ssd1306_init_panel();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SSD1306 init failed: %s", esp_err_to_name(err));
        return err;
    }

    display_ready = true;
    app_display_refresh();
    ESP_LOGI(TAG, "Heltec OLED initialized");
    return ESP_OK;
}

esp_err_t app_display_start() {
    BaseType_t created =
        xTaskCreate(display_task, "app_display", 4096, NULL, 3, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

#else

esp_err_t app_display_init() { return ESP_OK; }

esp_err_t app_display_start() { return ESP_OK; }

#endif
