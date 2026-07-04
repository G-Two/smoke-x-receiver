#ifndef APP_IMPROV_H
#define APP_IMPROV_H

/*
 * Improv-Serial provisioning task. Listens on UART0 for Improv Wi-Fi
 * frames sent by ESP Web Tools (https://www.improv-wifi.com/serial/) and
 * hands off received credentials to app_wifi for a hot-connect.
 *
 * Coexists with ESP_LOG output on UART0; Improv frames are well-framed
 * binary packets with magic bytes, so log lines interleaved between
 * frames are harmless. Frames are written atomically.
 */

#include <esp_err.h>

esp_err_t app_improv_start(void);

#endif
