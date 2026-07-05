#include "identify.h"
#include "items.h"
#include "rng.h"

static const uint8_t CLS_N[IDC_COUNT] = {
    N_POTIONS, N_SCROLLS, N_WANDS, N_RINGS
};

/* Non-static so the headless tests can inspect identification state. */
uint8_t  g_id_alias[IDC_COUNT][14];
uint16_t g_id_known[IDC_COUNT];
#define alias g_id_alias
#define known g_id_known

void identify_new_game(void) {
    uint8_t c, i;
    for (c = 0; c < IDC_COUNT; c++) {
        uint8_t n = CLS_N[c];
        for (i = 0; i < n; i++) alias[c][i] = i;
        /* Fisher-Yates */
        for (i = (uint8_t)(n - 1u); i > 0; i--) {
            uint8_t j = rng_range((uint8_t)(i + 1u));
            uint8_t t = alias[c][i];
            alias[c][i] = alias[c][j];
            alias[c][j] = t;
        }
        known[c] = 0;
    }
}

uint8_t identify_alias(uint8_t cls, uint8_t sub) {
    return alias[cls][sub];
}

uint8_t identify_known(uint8_t cls, uint8_t sub) {
    return (known[cls] >> sub) & 1u;
}

void identify_learn(uint8_t cls, uint8_t sub) {
    known[cls] |= (uint16_t)(1u << sub);
}
