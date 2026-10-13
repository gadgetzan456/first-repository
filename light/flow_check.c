/* flowchart_fixed.drawio の流れを、そのまま C に書き写したもの。
 * light.c（テスト済み）とランダム入力で比べ、LED の出力が毎回一致するかを確かめる。
 *   cc -std=c99 -Wall -Wextra -o flow_check light.c flow_check.c && ./flow_check
 *   ./flow_check trace   … 動作トレース（図の3ページ目）を出力
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "light.h"

/* ===== フローチャートの変数（すべてグローバル） ===== */
static volatile int short_flag, long_flag, blink_flag; /* 割り込み → main の合図 */
static volatile int mode;                              /* main が書き、割り込みも読む */
static int sw_state, sw_cnt, press_time, long_done, short_cnt, blink_cnt; /* 割り込み側 */
static int i, blink_on;                                                     /* main 側 */
static int LED[3];

static int sim_switch;      /* スイッチの生入力（両方の実装で共通） */
static int trace_on;
static long now_ms;

#define TRACE(...) do { if (trace_on) { printf("%6ld\t", now_ms); printf(__VA_ARGS__); \
    printf("\t%d%d%d\n", LED[0], LED[1], LED[2]); } } while (0)

/* ----- main：ループに入るまで ----- */
static void flow_main_init(void)
{
    LED[0] = LED[1] = LED[2] = 0;                 /* ポート初期化（全消灯） */
    i = mode = blink_on = 0;                      /* 変数の初期化 */
    short_flag = long_flag = blink_flag = 0;
    sw_state = sw_cnt = press_time = long_done = 0;
    short_cnt = blink_cnt = 0;
    /* タイマ設定・割り込み許可（シミュレーションでは呼び出し側が 10ms ごとに flow_isr を呼ぶ） */
}

/* ----- main：無限ループの1周 ----- */
static void flow_main_loop_once(void)
{
    if (short_flag == 1) {
        short_flag = 0;
        if (i == 3) {
            LED[2] = 0;
            blink_on = 0;
            mode = 1;
            TRACE("main\tshort_flag を処理 → LED3 消灯、mode = 1（点滅開始・短押し無効）");
        } else {
            if (i != 0) {
                LED[i - 1] = 0;
            }
            LED[i] = 1;
            i++;
            TRACE("main\tshort_flag を処理 → LED%d 点灯（i = %d）", i, i);
        }
    }
    if (long_flag == 1) {
        long_flag = 0;
        if (mode == 1) {
            mode = 0;
            blink_flag = 0;
            i = 0;
            blink_on = 0;
            LED[0] = LED[1] = LED[2] = 0;
            TRACE("main\tlong_flag を処理 → mode = 0、全消灯（短押し有効に戻る）");
        } else {
            TRACE("main\tlong_flag を処理 → 点滅中ではないので何もしない");
        }
    }
    if (blink_flag == 1) {
        blink_flag = 0;
        blink_on = 1 - blink_on;
        LED[0] = LED[1] = LED[2] = blink_on;
        TRACE("main\tblink_flag を処理 → 全LED %s", blink_on ? "点灯" : "消灯");
    }
}

/* ----- 10ms タイマ割り込み ----- */
static void flow_isr(void)
{
    /* 割り込み要因フラグのクリア（シミュレーションでは不要） */
    int sw = sim_switch;

    if (sw == sw_state) {
        sw_cnt = 0;
    } else {
        sw_cnt++;
        if (sw_cnt >= 3) {
            sw_cnt = 0;
            sw_state = sw;
            if (sw_state == 1) {
                press_time = 0;
                long_done = 0;
                TRACE("割り込み\t押下が確定（sw_state = 1）");
            } else {
                TRACE("割り込み\t離したことが確定（sw_state = 0、押下時間 %d0ms）", press_time);
                if (long_done == 0) {
                    if (mode == 0) {
                        short_cnt++;
                        TRACE("割り込み\t短押し → short_cnt = %d", short_cnt);
                        if (short_cnt >= 2) {
                            short_cnt = 0;
                            short_flag = 1;
                            TRACE("割り込み\tshort_cnt = 0、short_flag = 1");
                        }
                    } else {
                        TRACE("割り込み\t短押しだが mode = 1（点滅中）なので数えない");
                    }
                }
            }
        }
    }

    if (sw_state == 1 && long_done == 0) {
        press_time++;
        if (press_time >= 300) {
            long_done = 1;
            long_flag = 1;
            TRACE("割り込み\tpress_time = 300（3秒）→ long_done = 1、long_flag = 1");
        }
    }

    if (mode == 1) {
        blink_cnt++;
        if (blink_cnt >= 50) {
            blink_cnt = 0;
            blink_flag = 1;
            TRACE("割り込み\tblink_cnt = 50（0.5秒）→ blink_flag = 1");
        }
    } else {
        blink_cnt = 0;
    }
}

/* ===== light.c 側 ===== */
static bool ref_led[3];
bool hal_switch_is_on(void) { return sim_switch != 0; }
void hal_led_write(uint8_t index, bool on) { ref_led[index] = on; }

/* 10ms 1回分：両方の実装を進めて比べる */
static int step(void)
{
    light_tick_10ms();
    flow_isr();
    flow_main_loop_once();      /* main は割り込みの合間に少なくとも1周する */
    now_ms += 10;
    for (int k = 0; k < 3; k++) {
        if ((int)ref_led[k] != LED[k]) {
            return 0;
        }
    }
    return 1;
}

static unsigned long rng = 12345;
static int rnd(int n) { rng = rng * 1103515245u + 12345u; return (int)((rng >> 16) % (unsigned)n); }

/* チャタリング（1〜2回ずつ交互、3回連続にはならない）をはさんで level に変える */
static int set_switch(int level, int chatter)
{
    if (chatter) {
        int v = level, runs = 3 + rnd(3);
        for (int r = 0; r < runs; r++) {
            int len = 1 + rnd(2);
            for (int k = 0; k < len; k++) {
                sim_switch = v;
                if (!step()) return 0;
            }
            v = !v;
        }
    }
    sim_switch = level;
    return 1;
}

static int hold(int ms)
{
    for (int t = 0; t < ms; t += 10) {
        if (!step()) return 0;
    }
    return 1;
}

static void trace_press(int ms)
{
    set_switch(1, 0);
    hold(ms);
    set_switch(0, 0);
    hold(100);
}

int main(int argc, char **argv)
{
    light_init();
    flow_main_init();

    if (argc > 1 && strcmp(argv[1], "trace") == 0) {
        trace_on = 1;
        trace_press(200);   /* 短押し1回目 */
        trace_press(200);   /* 短押し2回目 → LED1 */
        trace_press(200);
        trace_press(200);   /* → LED2 */
        trace_press(200);
        trace_press(200);   /* → LED3 */
        trace_press(200);
        trace_press(200);   /* → LED3 消灯、点滅開始 */
        trace_press(200);   /* 点滅中の短押し（無視される） */
        hold(600);
        trace_press(3500);  /* 3秒長押し → 解除 */
        return 0;
    }

    /* ランダム入力で長時間比べる */
    long ticks_checked = 0;
    for (int n = 0; n < 200000; n++) {
        int kind = rnd(10);
        int ms = kind < 7 ? 30 + rnd(500)                 /* 短押し（チャタリング判定の境目も含む） */
               : kind < 9 ? 2900 + rnd(200)               /* 3秒前後の境目 */
               : 3000 + rnd(2000);                        /* 長押し */
        int chatter = rnd(2);
        if (!set_switch(1, chatter) || !hold(ms / 10 * 10) ||
            !set_switch(0, chatter) || !hold(10 + rnd(800) / 10 * 10)) {
            printf("不一致：%ld ms の時点で LED が食い違いました\n", now_ms);
            return EXIT_FAILURE;
        }
        ticks_checked = now_ms / 10;
    }
    printf("一致：%ld 回（約 %.1f 時間ぶん）の 10ms 周期で LED 出力がすべて light.c と一致\n",
           ticks_checked, ticks_checked / 100.0 / 3600.0);
    return EXIT_SUCCESS;
}
