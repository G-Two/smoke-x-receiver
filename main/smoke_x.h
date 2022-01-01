#ifndef SMOKE_X_H
#define SMOKE_X_H

#include <stdbool.h>
#include <esp_event.h>

#define SMOKE_X_APP_VERSION "0.1.0"

ESP_EVENT_DECLARE_BASE(SMOKE_X_EVENT);
#define SMOKE_X_DEVICE_ID_LEN 8
typedef enum {
    SMOKE_X_EVENT_SYNC = 0,
    SMOKE_X_EVENT_SYNC_SUCCESS,
    SMOKE_X_EVENT_STATE_X2,
    SMOKE_X_EVENT_STATE_X4,
    SMOKE_X_EVENT_DISCOVERY_REQUIRED,
} smoke_x_event_t;

typedef struct {
    unsigned int frequency;
    char device_id[SMOKE_X_DEVICE_ID_LEN];
} smoke_x_config_t;

typedef struct {               // Comments below indicate received format
    unsigned int unk_1;        // Normally 30
    char * units;              // F(1) or C(2)
    unsigned int unk_2;        // Normally 1, sometimes 2 (alarm?)
    bool probe_1_attached;     // probe1 inserted(0), not(3)
    double probe_1_temp;       // probe1 temp tenths_deg
    bool probe_1_alarm;        // probe1 alarm on (1), off(0)
    unsigned int probe_1_max;  // probe1 alarm max deg
    unsigned int probe_1_min;  // probe1 alarm min deg
    bool probe_2_attached;     // probe2 inserted(0), not(3)
    double probe_2_temp;       // probe2 temp tenths_deg
    bool probe_2_alarm;        // probe2 alarm on (1), off(0)
    union {
        int probe_2_max;     // probe2 alarm max
        int billows_target;  // billows set temp
    };
    int probe_2_min;        // probe2 alarm min deg
    bool billows_attached;  // billows attached(1), not(0)
    int unk_3;              // 0 or 4
} smoke_x_state_t;

esp_err_t smoke_x_init();
bool smoke_x_is_configured();
esp_err_t smoke_x_sync();
esp_err_t smoke_x_start();
esp_err_t smoke_x_stop();
esp_err_t smoke_x_get_config(smoke_x_config_t *p_config);
esp_err_t smoke_x_get_state(smoke_x_state_t *p_state);
char * smoke_x_get_units();
char * smoke_x_get_device_id();

#endif
