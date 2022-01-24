#ifndef SMOKE_X_H
#define SMOKE_X_H

#include <stdbool.h>
#include <esp_event.h>

#define SMOKE_X_APP_VERSION "0.1.0"
#define SMOKE_X_DEVICE_ID_LEN 8
#define SMOKE_X_PROBE_1 "probe_1"
#define SMOKE_X_PROBE_2 "probe_2"
#define SMOKE_X_PROBE_3 "probe_3"
#define SMOKE_X_PROBE_4 "probe_4"
#define SMOKE_X_BILLOWS "billows"
#define SMOKE_X_ALARM_MAX "alarm_max"
#define SMOKE_X_ALARM_MIN "alarm_min"
#define SMOKE_X_CURRENT_TEMP "current_temp"
#define SMOKE_X_HISTORY "history"

ESP_EVENT_DECLARE_BASE(SMOKE_X_EVENT);
typedef enum {
    SMOKE_X_EVENT_SYNC = 0,
    SMOKE_X_EVENT_SYNC_SUCCESS,
    SMOKE_X_EVENT_STATE_MSG_RECEIVED,
    SMOKE_X_EVENT_DISCOVERY_REQUIRED,
} smoke_x_event_t;

typedef struct {
    unsigned int frequency;
    char device_id[SMOKE_X_DEVICE_ID_LEN];
    unsigned int num_probes;
} smoke_x_config_t;

typedef struct {
    bool attached;
    double temp;
    bool alarm;
    union {
        int max_temp;
        int billows_target;
    };
    int min_temp;
} smoke_x_probe_t;

typedef struct {
    unsigned int num_probes;
    char *units;
    bool new_alarm;
    bool billows_attached;
    smoke_x_probe_t probes[4];
} smoke_x_state_t;

esp_err_t smoke_x_init();
bool smoke_x_is_configured();
esp_err_t smoke_x_sync();
esp_err_t smoke_x_start();
esp_err_t smoke_x_stop();
esp_err_t smoke_x_get_config(smoke_x_config_t *p_config);
esp_err_t smoke_x_get_state(smoke_x_state_t *p_state);
unsigned int smoke_x_get_num_records();
char *smoke_x_get_data_json();
char *smoke_x_get_units();
char *smoke_x_get_device_id();

#endif
