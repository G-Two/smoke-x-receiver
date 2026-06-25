#ifndef SMOKE_X_PARSER_H
#define SMOKE_X_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include "smoke_x_types.h"

/*
 * Pure-C parser for Smoke X LoRa packets. No ESP-IDF dependencies — testable
 * on the host. The controller in smoke_x.c wraps these calls with event
 * dispatch, NVS, and history tracking.
 *
 * Sync message (sent by the base station during pairing):
 *   <?>,<deviceid>,<freq_b0>,<freq_b1>,<freq_b2>,<freq_b3>,
 *   - freq bytes are decimal-encoded little-endian uint32 of the operating
 *     frequency in Hz (e.g. 920000000)
 *
 * Sync ACK we transmit back:
 *   <deviceid>,SUCCESS,
 *
 * State message (X2; X4 has 4 probes instead of 2):
 *   <deviceid>,<?>,<units>,<alarm>,
 *     <p1_state>,<p1_temp_x10>,<p1_alarm>,<p1_max>,<p1_min>,
 *     <p2_state>,<p2_temp_x10>,<p2_alarm>,<p2_max>,<p2_min>,
 *     <billows>,<?>,
 *
 * - units: 1 -> "°F", else -> "°C"
 * - pN_state: 3 -> probe detached, else -> attached
 * - pN_temp_x10: temperature * 10 (e.g. "848" -> 84.8)
 */

#define SMOKE_X_PARSER_NUM_COMMAS_SYNC 6
#define SMOKE_X_PARSER_NUM_COMMAS_SUCCESS 2
#define SMOKE_X_PARSER_NUM_COMMAS_X2 16
#define SMOKE_X_PARSER_NUM_COMMAS_X4 26

/* Count comma occurrences. Used to discriminate sync vs X2 vs X4 messages. */
unsigned int smoke_x_parser_count_commas(const char *msg);

/*
 * Parse a sync message from the base station. Returns 0 on success, -1 if
 * any field is missing or inputs are NULL. On success `out->frequency` holds
 * the operating frequency in Hz (caller must still validate the range) and
 * `out->device_id` is a null-terminated string.
 */
int smoke_x_parser_parse_sync(const char *msg, smoke_x_sync_t *out);

/*
 * Parse a state message of the given probe count (2 or 4). Returns 0 on
 * success, -1 if any field is missing, num_probes is invalid, or inputs are
 * NULL. On success `out` is fully populated; `out->units` points to static
 * storage and is stable across calls (the controller relies on pointer
 * equality to detect unit changes).
 */
int smoke_x_parser_parse_state(const char *msg, unsigned int num_probes,
                               smoke_x_state_t *out);

/*
 * Format the sync acknowledgement ("<device_id>,SUCCESS,") into `buf`.
 * Returns the number of bytes written (excluding the terminating NUL) on
 * success, or -1 on invalid input / insufficient buffer.
 */
int smoke_x_parser_format_success(const char *device_id, char *buf,
                                  size_t buf_size);

#endif
