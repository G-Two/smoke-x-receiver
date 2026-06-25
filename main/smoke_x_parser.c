#include "smoke_x_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int smoke_x_parser_count_commas(const char *msg) {
    if (!msg) return 0;
    unsigned int n = 0;
    for (const char *p = msg; *p; p++) {
        if (*p == ',') n++;
    }
    return n;
}

int smoke_x_parser_parse_sync(const char *msg, smoke_x_sync_t *out) {
    if (!msg || !out) return -1;

    size_t msg_len = strlen(msg);
    char *tmp = (char *)malloc(msg_len + 1);
    if (!tmp) return -1;
    memcpy(tmp, msg, msg_len + 1);

    char *t;
#define NEXT_TOK(first)           \
    do {                          \
        t = strtok((first), ","); \
        if (!t) goto fail;        \
    } while (0)

    NEXT_TOK(tmp); /* unknown first field */

    NEXT_TOK(NULL); /* device id */
    strncpy(out->device_id, t, SMOKE_X_DEVICE_ID_LEN - 1);
    out->device_id[SMOKE_X_DEVICE_ID_LEN - 1] = '\0';

    unsigned char bytes[4];
    for (int i = 0; i < 4; i++) {
        NEXT_TOK(NULL);
        bytes[i] = (unsigned char)atoi(t);
    }

#undef NEXT_TOK

    /* Four bytes are sent in little-endian order. Compose explicitly so the
       result is independent of host endianness — matters for the host tests. */
    out->frequency = (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8) |
                     ((unsigned int)bytes[2] << 16) |
                     ((unsigned int)bytes[3] << 24);

    free(tmp);
    return 0;

fail:
    free(tmp);
    return -1;
}

int smoke_x_parser_format_success(const char *device_id, char *buf,
                                  size_t buf_size) {
    if (!device_id || !buf || buf_size == 0) return -1;
    int n = snprintf(buf, buf_size, "%s,SUCCESS,", device_id);
    if (n < 0 || (size_t)n >= buf_size) return -1;
    return n;
}

int smoke_x_parser_parse_state(const char *msg, unsigned int num_probes,
                               smoke_x_state_t *out) {
    if (!msg || !out || (num_probes != 2 && num_probes != 4)) return -1;

    size_t msg_len = strlen(msg);
    char *tmp = (char *)malloc(msg_len + 1);
    if (!tmp) return -1;
    memcpy(tmp, msg, msg_len + 1);

    out->num_probes = num_probes;

    char *t;
#define NEXT_TOK(first)           \
    do {                          \
        t = strtok((first), ","); \
        if (!t) goto fail;        \
    } while (0)

    NEXT_TOK(tmp);  /* device id (unused) */
    NEXT_TOK(NULL); /* unknown (unused) */

    NEXT_TOK(NULL);
    /* String literals have static storage so callers can rely on pointer
       stability (controller uses pointer-equality to detect unit changes). */
    out->units = atoi(t) == 1 ? "°F" : "°C";

    NEXT_TOK(NULL);
    out->new_alarm = atoi(t) != 0;

    for (unsigned int i = 0; i < num_probes; i++) {
        NEXT_TOK(NULL);
        out->probes[i].attached = atoi(t) != 3;
        NEXT_TOK(NULL);
        out->probes[i].temp = atof(t) / 10.0;
        NEXT_TOK(NULL);
        out->probes[i].alarm = atoi(t) != 0;
        NEXT_TOK(NULL);
        out->probes[i].max_temp = atoi(t);
        NEXT_TOK(NULL);
        out->probes[i].min_temp = atoi(t);
    }

    NEXT_TOK(NULL);
    out->billows_attached = atoi(t) != 0;

    NEXT_TOK(NULL); /* trailing unknown field (unused) */
#undef NEXT_TOK

    free(tmp);
    return 0;

fail:
    free(tmp);
    return -1;
}
