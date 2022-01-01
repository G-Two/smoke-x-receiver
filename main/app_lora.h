#ifndef APP_LORA_H
#define APP_LORA_H

#include <sys/types.h>
#include <freertos/task.h>

#define FREQ_MIN 137000000
#define FREQ_MAX 1020000000
#define BW_MIN 7800
#define BW_MAX 500000
#define SF_MIN 6
#define SF_MAX 12
#define PREAMBLE_MIN 6
#define PREAMBLE_MAX 65535
#define SYNC_WORD_MIN 0
#define SYNC_WORD_MAX 255
#define TX_POWER_MIN 2
#define TX_POWER_MAX 17
#define CR_MIN 5
#define CR_MAX 8
#define PAYLOAD_LEN_MAX 255
#define DEFAULT_FREQ 910500000
#define DEFAULT_BW 125000
#define DEFAULT_SF 9
#define DEFAULT_PREAMBLE_LEN 10
#define DEFAULT_SYNC_WORD 0x12
#define DEFAULT_TX_POWER 12
#define DEFAULT_CR 5
#define DEFAULT_MSG_LEN 30

typedef struct {
    uint8_t tx_power;
    uint32_t frequency;
    uint32_t bandwidth;
    uint8_t spreading_factor;
    uint8_t preamble_len;
    uint8_t sync_word;
    bool implicit_hdr;
    uint8_t msg_len;
    uint8_t coding_rate;
    bool crc_on;
} app_lora_params_t;

typedef struct {
    char* msg;
    uint32_t repeat_interval_ms;
    TaskHandle_t sending_task;
} app_lora_tx_msg_t;

int app_lora_start_tx(app_lora_tx_msg_t* task_arg);
int app_lora_start_rx(void (*cb)(const char*, const int));
int app_lora_stop_tx();
int app_lora_stop_rx();
int app_lora_get_params(app_lora_params_t* out_params);
int app_lora_set_params(app_lora_params_t* in_params, xTaskHandle calling_task);
int app_lora_init();

#endif
