#include "improv.h"

#include <string.h>

static const uint8_t IMPROV_MAGIC[6] = {'I', 'M', 'P', 'R', 'O', 'V'};

/* Sum bytes mod 256 — Improv's checksum. */
static uint8_t improv_checksum(const uint8_t *bytes, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += bytes[i];
    return (uint8_t)(sum & 0xff);
}

void improv_parser_reset(improv_parser_t *p) { p->pos = 0; }

/* Try to interpret p->buf[0..p->pos] as a complete frame. Returns:
 *   -1 if the prefix is invalid (caller should advance by 1 and retry)
 *    0 if the prefix is valid but incomplete (wait for more bytes)
 *    1 if a complete frame is recognized (parsed into out, parser reset) */
static int improv_try_consume(improv_parser_t *p, improv_rpc_t *out) {
    /* Always re-verify the magic — after a resync drop the buffer may hold
     * 9+ bytes of mid-frame garbage, so "enough bytes buffered" does not
     * imply the prefix was ever validated. */
    for (size_t i = 0; i < p->pos && i < sizeof(IMPROV_MAGIC); i++) {
        if (p->buf[i] != IMPROV_MAGIC[i]) return -1;
    }
    if (p->pos >= 7 && p->buf[6] != 1 /* version */) return -1;
    /* Need at least the magic + version + type + length. */
    if (p->pos < 9) return 0;

    if (p->buf[7] != IMPROV_TYPE_RPC) {
        /* We only consume RPCs sent from host -> device. Other frame
         * types (state/error/result) are device -> host only and
         * shouldn't appear inbound; treat as garbage. */
        return -1;
    }

    uint8_t payload_len = p->buf[8];
    size_t total = 9 + payload_len + 1; /* +1 for checksum */
    if (total > sizeof(p->buf)) return -1;
    if (p->pos < total) return 0;

    uint8_t cs = improv_checksum(p->buf, total - 1);
    if (cs != p->buf[total - 1]) return -1;

    /* Frame is valid. payload[0] = command, payload[1] = inner length,
     * payload[2..] = command-specific bytes. */
    if (payload_len < 2) return -1;

    uint8_t cmd = p->buf[9];
    uint8_t inner_len = p->buf[10];
    if ((size_t)inner_len + 2 > payload_len) return -1;

    memset(out, 0, sizeof(*out));
    out->cmd = cmd;

    if (cmd == IMPROV_CMD_WIFI_SETTINGS) {
        /* Inner layout: [ssid_len][ssid bytes][pwd_len][pwd bytes] */
        const uint8_t *body = &p->buf[11];
        size_t i = 0;
        if (i + 1 > inner_len) return -1;
        uint8_t ssid_len = body[i++];
        if (ssid_len >= sizeof(out->ssid)) return -1;
        if (i + ssid_len > inner_len) return -1;
        memcpy(out->ssid, &body[i], ssid_len);
        out->ssid[ssid_len] = '\0';
        i += ssid_len;
        if (i + 1 > inner_len) return -1;
        uint8_t pwd_len = body[i++];
        if (pwd_len >= sizeof(out->password)) return -1;
        if (i + pwd_len > inner_len) return -1;
        memcpy(out->password, &body[i], pwd_len);
        out->password[pwd_len] = '\0';
    }

    /* Any leftover bytes (shouldn't happen with well-formed input) are
     * shifted to the front so the next call sees them fresh. */
    size_t leftover = p->pos - total;
    if (leftover > 0) memmove(p->buf, &p->buf[total], leftover);
    p->pos = leftover;
    return 1;
}

bool improv_parse_byte(improv_parser_t *p, uint8_t b, improv_rpc_t *out) {
    if (p->pos < sizeof(p->buf)) {
        p->buf[p->pos++] = b;
    } else {
        /* Buffer overrun — reset and start fresh. */
        p->pos = 0;
        return false;
    }

    for (;;) {
        int r = improv_try_consume(p, out);
        if (r == 1) return true;
        if (r == 0) return false;
        /* r == -1: invalid prefix. Drop the first byte and retry — the
         * magic might start later in the buffer. */
        if (p->pos == 0) return false;
        memmove(p->buf, &p->buf[1], p->pos - 1);
        p->pos--;
        if (p->pos == 0) return false;
    }
}

/* Frame the given payload into out_buf. Returns frame length on success,
 * 0 if the buffer is too small or payload_len exceeds protocol max. */
static size_t improv_build_frame(uint8_t *out_buf, size_t out_buf_len,
                                 uint8_t type, const uint8_t *payload,
                                 uint8_t payload_len) {
    size_t total = 6 + 1 + 1 + 1 + payload_len + 1;
    if (out_buf_len < total) return 0;
    memcpy(out_buf, IMPROV_MAGIC, 6);
    out_buf[6] = 1; /* version */
    out_buf[7] = type;
    out_buf[8] = payload_len;
    if (payload_len > 0 && payload) {
        memcpy(&out_buf[9], payload, payload_len);
    }
    out_buf[9 + payload_len] = improv_checksum(out_buf, 9 + payload_len);
    return total;
}

size_t improv_build_state(uint8_t *out_buf, size_t out_buf_len, uint8_t state) {
    return improv_build_frame(out_buf, out_buf_len, IMPROV_TYPE_CURRENT_STATE,
                              &state, 1);
}

size_t improv_build_error(uint8_t *out_buf, size_t out_buf_len, uint8_t err) {
    return improv_build_frame(out_buf, out_buf_len, IMPROV_TYPE_ERROR_STATE,
                              &err, 1);
}

size_t improv_build_rpc_result_string(uint8_t *out_buf, size_t out_buf_len,
                                      uint8_t cmd, const char *str) {
    size_t slen = strlen(str);
    if (slen > 250) return 0; /* Leave room for headers */
    /* RPC_RESULT payload: [cmd][inner_len][str_len][str_bytes...] */
    uint8_t inner[256];
    inner[0] = cmd;
    inner[1] = (uint8_t)(slen + 1); /* inner length = 1 byte str_len + str */
    inner[2] = (uint8_t)slen;
    memcpy(&inner[3], str, slen);
    return improv_build_frame(out_buf, out_buf_len, IMPROV_TYPE_RPC_RESULT,
                              inner, (uint8_t)(slen + 3));
}
