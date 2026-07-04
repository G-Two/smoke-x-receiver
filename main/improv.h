#ifndef IMPROV_H
#define IMPROV_H

/*
 * Minimal implementation of the Improv Wi-Fi Serial protocol
 * (https://www.improv-wifi.com/serial/).
 *
 * The protocol is a small binary framing layer on top of UART used by
 * ESP Web Tools to provision Wi-Fi credentials from a browser after
 * flashing.
 *
 * Frame layout:
 *   "IMPROV" (6 bytes)
 *   <version=1>       (1 byte)
 *   <type>            (1 byte)
 *   <length>          (1 byte)
 *   <payload>         (length bytes)
 *   <checksum>        (1 byte)  — sum of every preceding byte, mod 256
 *
 * Only the subset needed for Wi-Fi provisioning is implemented:
 *   - parse a complete RPC frame from the host (WIFI_SETTINGS, IDENTIFY,
 *     GET_CURRENT_STATE, GET_DEVICE_INFO, GET_WIFI_NETWORKS)
 *   - build state / error / rpc-result frames to send back
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IMPROV_MAX_PAYLOAD 254
#define IMPROV_FRAME_MAX (6 + 1 + 1 + 1 + IMPROV_MAX_PAYLOAD + 1)

#define IMPROV_TYPE_CURRENT_STATE 0x01
#define IMPROV_TYPE_ERROR_STATE 0x02
#define IMPROV_TYPE_RPC 0x03
#define IMPROV_TYPE_RPC_RESULT 0x04

#define IMPROV_STATE_AUTHORIZED 0x02
#define IMPROV_STATE_PROVISIONING 0x03
#define IMPROV_STATE_PROVISIONED 0x04

#define IMPROV_ERR_NONE 0x00
#define IMPROV_ERR_INVALID_RPC 0x01
#define IMPROV_ERR_UNKNOWN_RPC 0x02
#define IMPROV_ERR_UNABLE_TO_CONNECT 0x03
#define IMPROV_ERR_NOT_AUTHORIZED 0x04

/* ESP Web Tools / sdk-cpp overload command 0x02 to mean both IDENTIFY
 * and GET_CURRENT_STATE — the browser sends 0x02 as its initial probe and
 * expects a state reply. We treat them as the same code; the dispatcher
 * always replies with current state on 0x02. */
#define IMPROV_CMD_WIFI_SETTINGS 0x01
#define IMPROV_CMD_GET_CURRENT_STATE 0x02
#define IMPROV_CMD_IDENTIFY 0x02
#define IMPROV_CMD_GET_DEVICE_INFO 0x03
#define IMPROV_CMD_GET_WIFI_NETWORKS 0x04

/* Parsed RPC command. ssid/password are NUL-terminated copies; populated
 * only for IMPROV_CMD_WIFI_SETTINGS. */
typedef struct {
    uint8_t cmd;
    char ssid[33];     /* 802.11 SSID is 32 bytes max + NUL */
    char password[64]; /* WPA passphrase is 63 bytes max + NUL */
} improv_rpc_t;

/* Byte-by-byte streaming parser. Hold one of these per UART. */
typedef struct {
    uint8_t buf[IMPROV_FRAME_MAX];
    size_t pos;
} improv_parser_t;

/* Reset a parser (call once before use, or to recover after an error). */
void improv_parser_reset(improv_parser_t *p);

/* Feed a byte into the parser. If a complete, valid RPC frame is
 * recognized, fills `out` and returns true. Returns false on incomplete
 * or invalid input (the parser self-recovers on bad magic / checksum). */
bool improv_parse_byte(improv_parser_t *p, uint8_t b, improv_rpc_t *out);

/* Frame builders. Each returns the total number of bytes written into
 * out_buf, or 0 if out_buf is too small. out_buf must be at least
 * IMPROV_FRAME_MAX bytes. */
size_t improv_build_state(uint8_t *out_buf, size_t out_buf_len, uint8_t state);
size_t improv_build_error(uint8_t *out_buf, size_t out_buf_len, uint8_t err);

/* Build an RPC_RESULT for the given command, carrying a single string
 * (e.g. the device URL returned after WIFI_SETTINGS succeeds). */
size_t improv_build_rpc_result_string(uint8_t *out_buf, size_t out_buf_len,
                                      uint8_t cmd, const char *str);

#endif
