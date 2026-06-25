#ifndef SMOKE_X_TYPES_H
#define SMOKE_X_TYPES_H

/*
 * Plain data types shared between firmware controller (smoke_x.c) and the
 * pure-C parser (smoke_x_parser.c). Kept ESP-IDF-free so the parser builds
 * cleanly in host unit tests.
 */

#include <stdbool.h>

#define SMOKE_X_DEVICE_ID_LEN 8

typedef struct {
    char device_id[SMOKE_X_DEVICE_ID_LEN]; /* null-terminated */
    unsigned int frequency;                /* Hz */
} smoke_x_sync_t;

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

#endif
