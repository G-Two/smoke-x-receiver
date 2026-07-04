/*
 * Host-only tests for the Improv-Serial framing layer. Built by
 * test/CMakeLists.txt against ../main/improv.c — no ESP-IDF required.
 */

#include "improv.h"

#include <stdio.h>
#include <string.h>

static int g_pass;
static int g_fail;

#define EXPECT(cond) do {                                                       \
    if (cond) {                                                                 \
        g_pass++;                                                               \
    } else {                                                                    \
        g_fail++;                                                               \
        fprintf(stderr, "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                           \
} while (0)

#define EXPECT_STR(a, b) EXPECT(strcmp((a), (b)) == 0)

#define RUN(name) do {                                                          \
    printf("== %s ==\n", #name);                                                \
    int before_fail = g_fail;                                                   \
    name();                                                                     \
    printf("   %s\n", g_fail == before_fail ? "ok" : "FAILED");                 \
} while (0)

/* --- helpers ------------------------------------------------------------ */

static uint8_t checksum(const uint8_t *bytes, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += bytes[i];
    return (uint8_t)(sum & 0xff);
}

/* Build a host->device RPC frame the way ESP Web Tools does:
 * IMPROV | ver | type=RPC | payload_len | [cmd][inner_len][inner...] | cs */
static size_t build_rpc(uint8_t *out, uint8_t cmd, const uint8_t *inner,
                        uint8_t inner_len) {
    size_t pos = 0;
    memcpy(out, "IMPROV", 6);
    pos = 6;
    out[pos++] = 1;
    out[pos++] = IMPROV_TYPE_RPC;
    out[pos++] = (uint8_t)(inner_len + 2);
    out[pos++] = cmd;
    out[pos++] = inner_len;
    memcpy(&out[pos], inner, inner_len);
    pos += inner_len;
    out[pos] = checksum(out, pos);
    return pos + 1;
}

/* WIFI_SETTINGS inner payload: [ssid_len][ssid][pwd_len][pwd] */
static size_t build_wifi_inner(uint8_t *inner, const char *ssid,
                               const char *pwd) {
    size_t pos = 0;
    size_t slen = strlen(ssid);
    size_t plen = strlen(pwd);
    inner[pos++] = (uint8_t)slen;
    memcpy(&inner[pos], ssid, slen);
    pos += slen;
    inner[pos++] = (uint8_t)plen;
    memcpy(&inner[pos], pwd, plen);
    pos += plen;
    return pos;
}

/* Feed a byte sequence into a parser; returns how many complete RPCs were
 * recognized, storing the last one in *out. */
static int feed(improv_parser_t *p, const uint8_t *bytes, size_t len,
                improv_rpc_t *out) {
    int frames = 0;
    for (size_t i = 0; i < len; i++) {
        if (improv_parse_byte(p, bytes[i], out)) frames++;
    }
    return frames;
}

/* --- parser tests -------------------------------------------------------- */

static void test_parse_wifi_settings(void) {
    uint8_t inner[128], frame[IMPROV_FRAME_MAX];
    size_t inner_len = build_wifi_inner(inner, "MyNetwork", "hunter2");
    size_t frame_len = build_rpc(frame, IMPROV_CMD_WIFI_SETTINGS, inner,
                                 (uint8_t)inner_len);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, frame, frame_len, &rpc) == 1);
    EXPECT(rpc.cmd == IMPROV_CMD_WIFI_SETTINGS);
    EXPECT_STR(rpc.ssid, "MyNetwork");
    EXPECT_STR(rpc.password, "hunter2");
}

static void test_parse_get_current_state(void) {
    /* ESP Web Tools' initial probe: cmd 0x02 with empty inner payload. */
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t frame_len = build_rpc(frame, IMPROV_CMD_GET_CURRENT_STATE, NULL, 0);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, frame, frame_len, &rpc) == 1);
    EXPECT(rpc.cmd == IMPROV_CMD_GET_CURRENT_STATE);
    EXPECT_STR(rpc.ssid, "");
    EXPECT_STR(rpc.password, "");
}

static void test_parse_empty_ssid_and_password(void) {
    uint8_t inner[8], frame[IMPROV_FRAME_MAX];
    size_t inner_len = build_wifi_inner(inner, "", "");
    size_t frame_len = build_rpc(frame, IMPROV_CMD_WIFI_SETTINGS, inner,
                                 (uint8_t)inner_len);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, frame, frame_len, &rpc) == 1);
    EXPECT_STR(rpc.ssid, "");
    EXPECT_STR(rpc.password, "");
}

static void test_parse_max_length_credentials(void) {
    char ssid[33], pwd[64];
    memset(ssid, 'S', 32);
    ssid[32] = '\0';
    memset(pwd, 'P', 63);
    pwd[63] = '\0';

    uint8_t inner[128], frame[IMPROV_FRAME_MAX];
    size_t inner_len = build_wifi_inner(inner, ssid, pwd);
    size_t frame_len = build_rpc(frame, IMPROV_CMD_WIFI_SETTINGS, inner,
                                 (uint8_t)inner_len);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, frame, frame_len, &rpc) == 1);
    EXPECT_STR(rpc.ssid, ssid);
    EXPECT_STR(rpc.password, pwd);
}

static void test_reject_oversized_ssid(void) {
    /* ssid_len 33 overflows the 32-byte SSID field; the frame is otherwise
     * well-formed, so the parser must reject it without producing an RPC. */
    uint8_t inner[64] = {0};
    inner[0] = 33;
    memset(&inner[1], 'S', 33);
    inner[34] = 0; /* pwd_len */
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t frame_len = build_rpc(frame, IMPROV_CMD_WIFI_SETTINGS, inner, 35);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, frame, frame_len, &rpc) == 0);
}

static void test_recover_from_garbage_prefix(void) {
    /* Boot-time ESP_LOG noise on the same UART lands before the frame. */
    const char *noise = "I (1234) app_wifi: some log line\r\n";
    uint8_t inner[64], frame[IMPROV_FRAME_MAX];
    size_t inner_len = build_wifi_inner(inner, "net", "pw");
    size_t frame_len = build_rpc(frame, IMPROV_CMD_WIFI_SETTINGS, inner,
                                 (uint8_t)inner_len);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, (const uint8_t *)noise, strlen(noise), &rpc) == 0);
    EXPECT(feed(&p, frame, frame_len, &rpc) == 1);
    EXPECT_STR(rpc.ssid, "net");
}

static void test_recover_from_bad_checksum(void) {
    uint8_t inner[64], frame[IMPROV_FRAME_MAX];
    size_t inner_len = build_wifi_inner(inner, "net", "pw");
    size_t frame_len = build_rpc(frame, IMPROV_CMD_WIFI_SETTINGS, inner,
                                 (uint8_t)inner_len);

    uint8_t corrupted[IMPROV_FRAME_MAX];
    memcpy(corrupted, frame, frame_len);
    corrupted[frame_len - 1] ^= 0xff;

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, corrupted, frame_len, &rpc) == 0);
    /* A clean frame afterwards must still parse. */
    EXPECT(feed(&p, frame, frame_len, &rpc) == 1);
    EXPECT_STR(rpc.ssid, "net");
}

static void test_recover_from_truncated_frame(void) {
    /* Half a frame, then the host retries with a complete one. */
    uint8_t inner[64], frame[IMPROV_FRAME_MAX];
    size_t inner_len = build_wifi_inner(inner, "net", "pw");
    size_t frame_len = build_rpc(frame, IMPROV_CMD_WIFI_SETTINGS, inner,
                                 (uint8_t)inner_len);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, frame, frame_len / 2, &rpc) == 0);
    /* The retry's "IMPROV" magic won't match the dangling prefix, forcing
     * the parser to resync before recognizing the complete frame. */
    EXPECT(feed(&p, frame, frame_len, &rpc) == 1);
    EXPECT_STR(rpc.ssid, "net");
}

static void test_reject_non_rpc_type(void) {
    /* Device->host frame types are garbage when inbound. */
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t frame_len = improv_build_state(frame, sizeof(frame),
                                          IMPROV_STATE_AUTHORIZED);
    EXPECT(frame_len > 0);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, frame, frame_len, &rpc) == 0);
}

static void test_back_to_back_frames(void) {
    uint8_t inner1[64], inner2[64];
    uint8_t frame1[IMPROV_FRAME_MAX], frame2[IMPROV_FRAME_MAX];
    size_t len1 = build_rpc(frame1, IMPROV_CMD_WIFI_SETTINGS, inner1,
                            (uint8_t)build_wifi_inner(inner1, "one", "pw1"));
    size_t len2 = build_rpc(frame2, IMPROV_CMD_WIFI_SETTINGS, inner2,
                            (uint8_t)build_wifi_inner(inner2, "two", "pw2"));

    uint8_t stream[2 * IMPROV_FRAME_MAX];
    memcpy(stream, frame1, len1);
    memcpy(stream + len1, frame2, len2);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    int frames = 0;
    /* Check the first frame completes with the right contents before the
     * second frame's bytes arrive. */
    for (size_t i = 0; i < len1 + len2; i++) {
        if (improv_parse_byte(&p, stream[i], &rpc)) {
            frames++;
            if (frames == 1) EXPECT_STR(rpc.ssid, "one");
        }
    }
    EXPECT(frames == 2);
    EXPECT_STR(rpc.ssid, "two");
}

static void test_sustained_garbage(void) {
    /* A long run of non-protocol bytes must never produce a frame and must
     * not wedge the parser. */
    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    int frames = 0;
    for (int i = 0; i < 4096; i++) {
        if (improv_parse_byte(&p, (uint8_t)(i * 7 + 13), &rpc)) frames++;
    }
    EXPECT(frames == 0);

    uint8_t inner[64], frame[IMPROV_FRAME_MAX];
    size_t inner_len = build_wifi_inner(inner, "net", "pw");
    size_t frame_len = build_rpc(frame, IMPROV_CMD_WIFI_SETTINGS, inner,
                                 (uint8_t)inner_len);
    EXPECT(feed(&p, frame, frame_len, &rpc) == 1);
}

static void test_reject_undersized_payload(void) {
    /* payload_len < 2 can't hold [cmd][inner_len]; must be rejected. */
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t pos = 0;
    memcpy(frame, "IMPROV", 6);
    pos = 6;
    frame[pos++] = 1;
    frame[pos++] = IMPROV_TYPE_RPC;
    frame[pos++] = 1; /* payload_len */
    frame[pos++] = IMPROV_CMD_GET_CURRENT_STATE;
    frame[pos] = checksum(frame, pos);
    pos++;

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, frame, pos, &rpc) == 0);
}

static void test_reject_inner_len_exceeding_payload(void) {
    /* inner_len claims more bytes than the payload carries. */
    uint8_t inner[4] = {0xaa, 0xbb};
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t frame_len = build_rpc(frame, IMPROV_CMD_WIFI_SETTINGS, inner, 2);
    frame[10] = 200; /* inner_len byte */
    frame[frame_len - 1] = checksum(frame, frame_len - 1);

    improv_parser_t p;
    improv_parser_reset(&p);
    improv_rpc_t rpc;
    EXPECT(feed(&p, frame, frame_len, &rpc) == 0);
}

/* --- builder tests -------------------------------------------------------- */

static void test_build_state_frame(void) {
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t n = improv_build_state(frame, sizeof(frame),
                                  IMPROV_STATE_PROVISIONED);
    EXPECT(n == 11); /* 6 magic + ver + type + len + 1 payload + cs */
    EXPECT(memcmp(frame, "IMPROV", 6) == 0);
    EXPECT(frame[6] == 1);
    EXPECT(frame[7] == IMPROV_TYPE_CURRENT_STATE);
    EXPECT(frame[8] == 1);
    EXPECT(frame[9] == IMPROV_STATE_PROVISIONED);
    EXPECT(frame[10] == checksum(frame, 10));
}

static void test_build_error_frame(void) {
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t n = improv_build_error(frame, sizeof(frame),
                                  IMPROV_ERR_UNABLE_TO_CONNECT);
    EXPECT(n == 11);
    EXPECT(frame[7] == IMPROV_TYPE_ERROR_STATE);
    EXPECT(frame[9] == IMPROV_ERR_UNABLE_TO_CONNECT);
    EXPECT(frame[10] == checksum(frame, 10));
}

static void test_build_rpc_result_string(void) {
    const char *url = "http://192.168.1.50/";
    size_t url_len = strlen(url);
    uint8_t frame[IMPROV_FRAME_MAX];
    size_t n = improv_build_rpc_result_string(frame, sizeof(frame),
                                              IMPROV_CMD_WIFI_SETTINGS, url);
    EXPECT(n == 9 + 3 + url_len + 1);
    EXPECT(frame[7] == IMPROV_TYPE_RPC_RESULT);
    EXPECT(frame[8] == 3 + url_len);           /* payload length */
    EXPECT(frame[9] == IMPROV_CMD_WIFI_SETTINGS);
    EXPECT(frame[10] == 1 + url_len);          /* inner length */
    EXPECT(frame[11] == url_len);              /* string length */
    EXPECT(memcmp(&frame[12], url, url_len) == 0);
    EXPECT(frame[n - 1] == checksum(frame, n - 1));
}

static void test_build_rejects_small_buffer(void) {
    uint8_t frame[8];
    EXPECT(improv_build_state(frame, sizeof(frame),
                              IMPROV_STATE_AUTHORIZED) == 0);
}

static void test_build_rejects_oversized_string(void) {
    char big[300];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    uint8_t frame[IMPROV_FRAME_MAX];
    EXPECT(improv_build_rpc_result_string(frame, sizeof(frame),
                                          IMPROV_CMD_WIFI_SETTINGS, big) == 0);
}

int main(void) {
    RUN(test_parse_wifi_settings);
    RUN(test_parse_get_current_state);
    RUN(test_parse_empty_ssid_and_password);
    RUN(test_parse_max_length_credentials);
    RUN(test_reject_oversized_ssid);
    RUN(test_recover_from_garbage_prefix);
    RUN(test_recover_from_bad_checksum);
    RUN(test_recover_from_truncated_frame);
    RUN(test_reject_non_rpc_type);
    RUN(test_back_to_back_frames);
    RUN(test_sustained_garbage);
    RUN(test_reject_undersized_payload);
    RUN(test_reject_inner_len_exceeding_payload);
    RUN(test_build_state_frame);
    RUN(test_build_error_frame);
    RUN(test_build_rpc_result_string);
    RUN(test_build_rejects_small_buffer);
    RUN(test_build_rejects_oversized_string);

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
