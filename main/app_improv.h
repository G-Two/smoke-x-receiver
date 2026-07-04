#ifndef APP_IMPROV_H
#define APP_IMPROV_H

/*
 * Improv-Serial provisioning task. Listens on UART0 for Improv Wi-Fi
 * frames sent by ESP Web Tools (https://www.improv-wifi.com/serial/) and
 * hands off received credentials to app_wifi for a hot-connect.
 *
 * Coexists with ESP_LOG output on UART0; Improv frames are well-framed
 * binary packets with magic bytes, so log lines between frames are harmless.
 * Note: ESP_LOG output may still interleave bytes with our writes on some targets.
 */

#include <esp_err.h>

esp_err_t app_improv_start(void);

#endif
