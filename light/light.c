#include "light.h"

#define TICK_MS            10u
#define DEBOUNCE_COUNT     3u                 /* 3回連続一致で確定 */
#define LONG_PRESS_TICKS   (3000u / TICK_MS)  /* 3秒以上を長押し */
#define BLINK_TICKS        (500u / TICK_MS)   /* 0.5秒ごとに点滅 */
#define PRESSES_PER_STEP   2u                 /* 2回短押しで1ステップ進む */

typedef enum {
    MODE_NORMAL, /* 短押し有効：LED1→2→3の順に点灯 */
    MODE_BLINK   /* 2周目：全LED点滅、短押し無効 */
} light_mode_t;

/* スイッチ（チャタリング防止後）の状態 */
static bool     sw_on;
static uint8_t  sw_match_count;
static uint16_t press_ticks;
static bool     long_press_done;

/* ライトの状態 */
static light_mode_t mode;
static uint8_t  short_press_count;
static uint8_t  step;        /* 0:全消灯, 1〜3:そのLEDだけ点灯 */
static uint16_t blink_ticks;
static bool     blink_on;

static void leds_write_all(bool on)
{
    for (uint8_t i = 0; i < LIGHT_LED_NUM; i++) {
        hal_led_write(i, on);
    }
}

static void leds_show_step(void)
{
    for (uint8_t i = 0; i < LIGHT_LED_NUM; i++) {
        hal_led_write(i, (uint8_t)(i + 1u) == step);
    }
}

static void enter_normal(void)
{
    mode = MODE_NORMAL;
    short_press_count = 0;
    step = 0;
    leds_show_step();
}

static void enter_blink(void)
{
    mode = MODE_BLINK;
    blink_ticks = 0;
    blink_on = true;
    leds_write_all(blink_on);
}

static void on_short_press(void)
{
    if (mode != MODE_NORMAL) {
        return; /* 点滅中は短押し無効 */
    }
    short_press_count++;
    if (short_press_count < PRESSES_PER_STEP) {
        return;
    }
    short_press_count = 0;
    if (step < LIGHT_LED_NUM) {
        step++;
        leds_show_step();
    } else {
        enter_blink(); /* LED3の次＝2周目に入った */
    }
}

static void on_long_press(void)
{
    if (mode == MODE_BLINK) {
        enter_normal(); /* 短押しを再び有効にする */
    }
}

/* 生入力が3回連続で現在と異なる値になったら状態を確定させる */
static void debounce_switch(void)
{
    bool raw = hal_switch_is_on();

    if (raw == sw_on) {
        sw_match_count = 0;
        return;
    }
    if (++sw_match_count < DEBOUNCE_COUNT) {
        return;
    }
    sw_match_count = 0;
    sw_on = raw;

    if (sw_on) {
        press_ticks = 0;
        long_press_done = false;
    } else if (!long_press_done) {
        on_short_press(); /* 3秒未満で離された */
    }
}

static void update_press_time(void)
{
    if (!sw_on || long_press_done) {
        return;
    }
    if (++press_ticks >= LONG_PRESS_TICKS) {
        long_press_done = true; /* 離したときに短押し扱いしない */
        on_long_press();
    }
}

static void update_blink(void)
{
    if (mode != MODE_BLINK) {
        return;
    }
    if (++blink_ticks >= BLINK_TICKS) {
        blink_ticks = 0;
        blink_on = !blink_on;
        leds_write_all(blink_on);
    }
}

void light_init(void)
{
    sw_on = false;
    sw_match_count = 0;
    press_ticks = 0;
    long_press_done = false;
    enter_normal();
}

void light_tick_10ms(void)
{
    update_blink(); /* 先に処理し、点滅開始直後の周期を0.5秒ちょうどにする */
    debounce_switch();
    update_press_time();
}
