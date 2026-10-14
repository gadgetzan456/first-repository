/* 演習フローチャート（訂正版：flowchart_訂正版.drawio）をそのまま C に書き写した確認用プログラム。
 * 配列の範囲外アクセス・カウンタのあふれが起きないことと、仕様どおりの動きを確かめる。
 *   cc -std=c99 -Wall -Wextra -o flow_check_user flow_check_user.c && ./flow_check_user
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* stic のあふれを短時間で試すため 16bit にしている（実機では unsigned int でよい） */
typedef uint16_t tick_t;

static volatile tick_t stic;                 /* 割り込みが 10ms ごとに +1 */
static volatile int Tflag, Nflag, tanoshi;   /* 割り込み → main */
static int cnt;                              /* 割り込み内の static */
static int i, blink_on;                      /* main */
static tick_t blink_start;
static int LED[3];
static int sw;                               /* スイッチ入力（1 = 押下） */
static int fail;

static void led(int k, int v)
{
    if (k < 0 || k > 2) {
        printf("NG: LED[%d] に書き込もうとした（範囲外）\n", k);
        exit(1);
    }
    LED[k] = v;
}

/* ===== 10msの割り込みタイマ ===== */
static void isr(void)
{
    stic++;
    if (sw) {                       /* スイッチ状態ON? Yes */
        if (cnt < 300) cnt++;       /* 300 で止める */
        return;
    }
    if (cnt >= 3) {                 /* ちゃんと押されていた */
        if (cnt >= 300) {
            Nflag = 1;
        } else {
            tanoshi++;
            if (tanoshi == 2) {
                Tflag = 1;
                tanoshi = 0;
            }
        }
    }
    cnt = 0;
}

/* ===== main ===== */
static void main_init(void)
{
    i = 0; tanoshi = 0; Tflag = 0; Nflag = 0;
    led(0, 0); led(1, 0); led(2, 0);
}

static void main_loop_once(void)
{
    if (Nflag == 1) {
        Nflag = 0;
        if (i == 4) {
            led(0, 0); led(1, 0); led(2, 0);
            i = 0;
            tanoshi = 0;
        }
        return;                     /* ループ先頭へ */
    }
    if (Tflag == 1) {
        Tflag = 0;
        if (i == 4) {
            /* 点滅中：何もしない */
        } else if (i == 3) {
            led(2, 0);
            blink_on = 0;
            blink_start = stic;
            i = 4;
        } else {
            if (i != 0) led(i - 1, 0);
            led(i, 1);
            i++;
        }
    }
    if (i == 4) {
        if ((tick_t)(stic - blink_start) >= 50) {
            blink_start = stic;
            blink_on = 1 - blink_on;
            led(0, blink_on); led(1, blink_on); led(2, blink_on);
        }
    }
}

/* ===== 試験 ===== */
static void tick(void) { isr(); main_loop_once(); if (cnt > 300 || i < 0 || i > 4) { printf("NG: cnt=%d i=%d\n", cnt, i); exit(1); } }
static void hold(int v, int ms) { sw = v; for (int t = 0; t < ms; t += 10) tick(); }
static void press(int ms) { hold(1, ms); hold(0, 100); }
static void expect(const char *name, int a, int b, int c)
{
    int ok = LED[0] == a && LED[1] == b && LED[2] == c;
    printf("[%s] %s: LED=%d%d%d\n", ok ? "OK" : "NG", name, LED[0], LED[1], LED[2]);
    fail += !ok;
}

int main(void)
{
    stic = 65000;                   /* 試験中に stic が 65535 → 0 とあふれるように */
    main_init();
    hold(0, 500);                   expect("何もしなければ全消灯", 0, 0, 0);
    press(200);                     expect("短押し1回では変化なし", 0, 0, 0);
    press(200);                     expect("短押し2回で LED1", 1, 0, 0);
    hold(1, 5000); hold(0, 100);    expect("点滅前の長押しは何もしない", 1, 0, 0);
    press(200); press(200);         expect("LED2", 0, 1, 0);
    press(200); press(200);         expect("LED3", 0, 0, 1);
    for (int k = 0; k < 3; k++) { sw = 1; tick(); sw = 0; tick(); }  /* チャタリング（ONが3回続かない） */
    hold(0, 100);                   expect("チャタリングは無視", 0, 0, 1);
    press(200); hold(1, 200); sw = 0; tick();
    expect("LED3 を消してすぐ点滅（消灯から）", 0, 0, 0);
    { int t = 0; while (LED[0] == 0 && t < 200) { tick(); t++; }
      printf("[%s] 最初の点灯まで %d0ms（期待 500ms 前後）\n", (t >= 49 && t <= 50) ? "OK" : "NG", t); fail += !(t >= 49 && t <= 50); }
    { int t = 0, before = LED[0]; while (LED[0] == before && t < 200) { tick(); t++; }
      printf("[%s] 次の切り替えまで %d0ms\n", t == 50 ? "OK" : "NG", t); fail += t != 50; }
    for (int k = 0; k < 5; k++) press(200);                       /* 点滅中に短押し5回（奇数） */
    printf("[%s] 点滅中の短押しで範囲外書き込みなし、i = %d\n", i == 4 ? "OK" : "NG", i); fail += i != 4;
    hold(1, 3500); hold(0, 30);     expect("3秒以上押して離すと全消灯", 0, 0, 0);
    hold(0, 1000);                  expect("解除後は点滅しない", 0, 0, 0);
    press(200);                     expect("解除後の短押し1回では変化なし（点滅中の余りは捨てた）", 0, 0, 0);
    press(200);                     expect("解除後の短押し2回で LED1", 1, 0, 0);
    printf("stic は試験中に 65535 → 0 とあふれた（現在 %u）\n", (unsigned)stic);

    /* ランダムな操作を長時間：範囲外アクセスや i・cnt の異常があれば exit(1) */
    srand(1);
    for (int n = 0; n < 300000; n++) {
        hold(1, (rand() % 6 == 0) ? 2800 + rand() % 3000 : 10 + rand() % 400);
        hold(0, 10 + rand() % 600);
    }
    printf("ランダム操作 30万回：範囲外アクセス・i や cnt の異常なし\n");
    printf(fail ? "\n%d 件失敗\n" : "\nすべて成功\n", fail);
    return fail != 0;
}
