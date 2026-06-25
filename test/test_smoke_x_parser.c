/*
 * Host-only tests for the Smoke X LoRa packet parser. Built by test/CMakeLists.txt
 * against ../main/smoke_x_parser.c — no ESP-IDF required.
 */

#include "smoke_x_parser.h"

#include <math.h>
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

#define EXPECT_FEQ(a, b) EXPECT(fabs((a) - (b)) < 1e-6)
#define EXPECT_STR(a, b) EXPECT(strcmp((a), (b)) == 0)

#define RUN(name) do {                                                          \
    printf("== %s ==\n", #name);                                                \
    int before_fail = g_fail;                                                   \
    name();                                                                     \
    printf("   %s\n", g_fail == before_fail ? "ok" : "FAILED");                 \
} while (0)

/* Real X2 packet captured from device log. */
static const char *X2_REAL =
    "|abCDe,30,1,1,0,848,1,125,32,0,849,0,260,195,0,0,";

static void test_count_commas_x2(void) {
    EXPECT(smoke_x_parser_count_commas(X2_REAL) == SMOKE_X_PARSER_NUM_COMMAS_X2);
}

static void test_count_commas_x4(void) {
    /* Synthetic X4: 4 unused header fields + 4 probes * 5 fields + 2 trailing = 26 commas */
    const char *x4 =
        "|abcde,30,1,0,"
        "0,700,0,200,100,"
        "0,710,0,200,100,"
        "0,720,0,200,100,"
        "0,730,0,200,100,"
        "0,0,";
    EXPECT(smoke_x_parser_count_commas(x4) == SMOKE_X_PARSER_NUM_COMMAS_X4);
}

static void test_count_commas_empty(void) {
    EXPECT(smoke_x_parser_count_commas("") == 0);
    EXPECT(smoke_x_parser_count_commas(NULL) == 0);
}

static void test_parse_x2_real_packet(void) {
    smoke_x_state_t out = {0};
    EXPECT(smoke_x_parser_parse_state(X2_REAL, 2, &out) == 0);
    EXPECT(out.num_probes == 2);
    EXPECT_STR(out.units, "°F");
    EXPECT(out.new_alarm == true);

    /* Probe 1: 0,848,1,125,32 */
    EXPECT(out.probes[0].attached == true);
    EXPECT_FEQ(out.probes[0].temp, 84.8);
    EXPECT(out.probes[0].alarm == true);
    EXPECT(out.probes[0].max_temp == 125);
    EXPECT(out.probes[0].min_temp == 32);

    /* Probe 2: 0,849,0,260,195 */
    EXPECT(out.probes[1].attached == true);
    EXPECT_FEQ(out.probes[1].temp, 84.9);
    EXPECT(out.probes[1].alarm == false);
    EXPECT(out.probes[1].max_temp == 260);
    EXPECT(out.probes[1].min_temp == 195);

    EXPECT(out.billows_attached == false);
}

static void test_parse_celsius(void) {
    /* Same fields but units field = 0 → °C */
    const char *raw = "|abCDe,30,0,0,0,250,0,125,32,0,251,0,260,195,0,0,";
    smoke_x_state_t out = {0};
    EXPECT(smoke_x_parser_parse_state(raw, 2, &out) == 0);
    EXPECT_STR(out.units, "°C");
    EXPECT_FEQ(out.probes[0].temp, 25.0);
}

static void test_parse_detached_probe(void) {
    /* Probe 2 state = 3 → detached */
    const char *raw = "|abCDe,30,1,0,0,848,0,125,32,3,0,0,0,0,0,0,";
    smoke_x_state_t out = {0};
    EXPECT(smoke_x_parser_parse_state(raw, 2, &out) == 0);
    EXPECT(out.probes[0].attached == true);
    EXPECT(out.probes[1].attached == false);
}

static void test_parse_billows_attached(void) {
    const char *raw = "|abCDe,30,1,0,0,848,0,125,32,0,849,0,260,195,1,0,";
    smoke_x_state_t out = {0};
    EXPECT(smoke_x_parser_parse_state(raw, 2, &out) == 0);
    EXPECT(out.billows_attached == true);
}

static void test_parse_x4(void) {
    const char *raw =
        "|abcde,30,1,0,"
        "0,700,0,200,100,"
        "0,710,0,200,100,"
        "0,720,0,200,100,"
        "0,730,0,200,100,"
        "0,0,";
    smoke_x_state_t out = {0};
    EXPECT(smoke_x_parser_parse_state(raw, 4, &out) == 0);
    EXPECT(out.num_probes == 4);
    EXPECT_FEQ(out.probes[0].temp, 70.0);
    EXPECT_FEQ(out.probes[1].temp, 71.0);
    EXPECT_FEQ(out.probes[2].temp, 72.0);
    EXPECT_FEQ(out.probes[3].temp, 73.0);
}

static void test_parse_sync(void) {
    /* From the docstring example: "020001,|abCDe,160,32,69,54,"
       Frequency LE: 160 | (32<<8) | (69<<16) | (54<<24)
                   = 910500000 Hz (910.5 MHz, in the 902–928 MHz range) */
    const char *raw = "020001,|abCDe,160,32,69,54,";
    smoke_x_sync_t out = {0};
    EXPECT(smoke_x_parser_parse_sync(raw, &out) == 0);
    EXPECT_STR(out.device_id, "|abCDe");
    EXPECT(out.frequency == 910500000U);
    EXPECT(smoke_x_parser_count_commas(raw) ==
           SMOKE_X_PARSER_NUM_COMMAS_SYNC);
}

static void test_parse_sync_rejects_bad_inputs(void) {
    smoke_x_sync_t out = {0};
    EXPECT(smoke_x_parser_parse_sync(NULL, &out) == -1);
    EXPECT(smoke_x_parser_parse_sync("020001,|abCDe,160,32,69,54,", NULL) == -1);
    EXPECT(smoke_x_parser_parse_sync("", &out) == -1);
    EXPECT(smoke_x_parser_parse_sync("020001,|abCDe,160,32,", &out) == -1);
}

static void test_format_success(void) {
    char buf[32];
    int n = smoke_x_parser_format_success("|abCDe", buf, sizeof(buf));
    EXPECT(n > 0);
    EXPECT_STR(buf, "|abCDe,SUCCESS,");
    EXPECT(n == (int)strlen("|abCDe,SUCCESS,"));
    EXPECT(smoke_x_parser_count_commas(buf) ==
           SMOKE_X_PARSER_NUM_COMMAS_SUCCESS);
}

static void test_format_success_rejects_overflow(void) {
    char buf[8];  /* too small for "|abCDe,SUCCESS," (15 bytes + NUL) */
    EXPECT(smoke_x_parser_format_success("|abCDe", buf, sizeof(buf)) == -1);
    EXPECT(smoke_x_parser_format_success(NULL, buf, sizeof(buf)) == -1);
    EXPECT(smoke_x_parser_format_success("|abCDe", NULL, sizeof(buf)) == -1);
    EXPECT(smoke_x_parser_format_success("|abCDe", buf, 0) == -1);
}

static void test_parse_rejects_bad_inputs(void) {
    smoke_x_state_t out = {0};
    EXPECT(smoke_x_parser_parse_state(NULL, 2, &out) == -1);
    EXPECT(smoke_x_parser_parse_state(X2_REAL, 2, NULL) == -1);
    EXPECT(smoke_x_parser_parse_state(X2_REAL, 3, &out) == -1);
    EXPECT(smoke_x_parser_parse_state("", 2, &out) == -1);
    EXPECT(smoke_x_parser_parse_state("|too,short", 2, &out) == -1);
}

static void test_units_pointer_is_stable(void) {
    /* The controller compares units by pointer to detect changes.
       Two parses of "°F" messages must yield the same pointer. */
    smoke_x_state_t a = {0}, b = {0};
    EXPECT(smoke_x_parser_parse_state(X2_REAL, 2, &a) == 0);
    EXPECT(smoke_x_parser_parse_state(X2_REAL, 2, &b) == 0);
    EXPECT(a.units == b.units);
}

int main(void) {
    RUN(test_count_commas_x2);
    RUN(test_count_commas_x4);
    RUN(test_count_commas_empty);
    RUN(test_parse_x2_real_packet);
    RUN(test_parse_celsius);
    RUN(test_parse_detached_probe);
    RUN(test_parse_billows_attached);
    RUN(test_parse_x4);
    RUN(test_parse_sync);
    RUN(test_parse_sync_rejects_bad_inputs);
    RUN(test_format_success);
    RUN(test_format_success_rejects_overflow);
    RUN(test_parse_rejects_bad_inputs);
    RUN(test_units_pointer_is_stable);

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
