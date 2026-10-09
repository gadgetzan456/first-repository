/* PC上で動作を確認するためのシミュレーションテスト
 *   cc -std=c99 -Wall -Wextra -o test_light light.c test_light.c && ./test_light
 */
#include <stdio.h>
#include <stdlib.h>
#include "light.h"

static bool sim_switch;
static bool sim_led[LIGHT_LED_NUM];
static int  failures;

bool hal_switch_is_on(void) { return sim_switch; }
void hal_led_write(uint8_t index, bool on) { sim_led[index] = on; }

static void run_ms(bool sw, unsigned ms)
{
    sim_switch = sw;
    for (unsigned t = 0; t < ms; t += 10) {
        light_tick_10ms();
    }
}

static int run_ms_count_toggles(bool sw, unsigned ms)
{
    int toggles = 0;
    sim_switch = sw;
    for (unsigned t = 0; t < ms; t += 10) {
        bool before = sim_led[0];
        light_tick_10ms();
        toggles += sim_led[0] != before;
    }
    return toggles;
}

static void short_press(void)
{
    run_ms(true, 100);
    run_ms(false, 100);
}

static void expect_leds(const char *name, bool l1, bool l2, bool l3)
{
    bool ok = sim_led[0] == l1 && sim_led[1] == l2 && sim_led[2] == l3;
    printf("[%s] %s: LED=%d%d%d (期待 %d%d%d)\n", ok ? "OK" : "NG", name,
           sim_led[0], sim_led[1], sim_led[2], l1, l2, l3);
    if (!ok) {
        failures++;
    }
}

/* 全LEDがそろって0.5秒ごとに反転することを count 回確認する */
static void expect_blink(const char *name, bool sw, int count)
{
    sim_switch = sw;
    for (int n = 0; n < count; n++) {
        bool before = sim_led[0];
        unsigned t = 0;
        do {
            light_tick_10ms();
            t += 10;
            if (sim_led[0] != sim_led[1] || sim_led[1] != sim_led[2]) {
                break;
            }
        } while (sim_led[0] == before && t < 2000);
        bool ok = t == 500 && sim_led[0] != before &&
                  sim_led[0] == sim_led[1] && sim_led[1] == sim_led[2];
        printf("[%s] %s: %ums後に LED=%d%d%d\n", ok ? "OK" : "NG", name, t,
               sim_led[0], sim_led[1], sim_led[2]);
        if (!ok) {
            failures++;
        }
    }
}

int main(void)
{
    light_init();
    expect_leds("初期状態は全消灯", 0, 0, 0);

    /* チャタリング：2回だけONでは押下と判定しない */
    for (int i = 0; i < 10; i++) {
        run_ms(true, 20);
        run_ms(false, 20);
    }
    run_ms(false, 100);
    short_press();
    expect_leds("チャタリング＋短押し1回では変化なし", 0, 0, 0);
    short_press();
    expect_leds("短押し2回でLED1", 1, 0, 0);

    /* 通常時の長押しは短押しとして数えない */
    run_ms(true, 3500);
    run_ms(false, 100);
    short_press();
    expect_leds("長押しは無視、短押し1回では変化なし", 1, 0, 0);
    short_press();
    expect_leds("短押し2回でLED1消灯", 0, 0, 0);
    short_press();
    expect_leds("短押し1回では変化なし", 0, 0, 0);
    short_press();
    expect_leds("短押し2回でLED2", 0, 1, 0);
    short_press();
    short_press();
    expect_leds("短押し2回でLED2消灯", 0, 0, 0);

    /* 2.99秒押しは短押し（ON/OFFとも確定が同じだけ遅れるので押下時間はそのまま） */
    run_ms(true, 2990);
    run_ms(false, 100);
    short_press();
    expect_leds("2.99秒押し＋短押しでLED3", 0, 0, 1);

    /* 2周目：短押し2回目を離した瞬間（OFF確定）に点滅開始 */
    run_ms(true, 100);
    run_ms(false, 100);
    run_ms(true, 100);
    run_ms(false, 20);
    expect_leds("離す直前はLED3のまま", 0, 0, 1);
    run_ms(false, 10);
    expect_leds("2周目突入で全点灯（点滅開始）", 1, 1, 1);
    expect_blink("全LEDが0.5秒ごとに点滅", false, 4);

    /* 点滅中の短押しは無効：押している間も周期が変わらない */
    expect_blink("点滅中にスイッチON（短押し）", true, 1);
    expect_blink("点滅中にスイッチOFF", false, 1);
    expect_blink("点滅中にスイッチON（短押し）", true, 1);
    expect_blink("点滅中にスイッチOFF", false, 3);
    run_ms(false, 490);
    expect_leds("点滅周期の終わりで全点灯", 1, 1, 1);

    /* 3秒長押しで短押し有効に戻る */
    run_ms(true, 20);   /* 次の10msで押下確定 */
    {
        int toggles = run_ms_count_toggles(true, 2990);
        bool ok = toggles == 6;
        printf("[%s] 確定から2.99秒間は点滅継続: 反転 %d 回（期待 6）\n",
               ok ? "OK" : "NG", toggles);
        failures += !ok;
    }
    run_ms(true, 10);
    expect_leds("3秒長押しで全消灯・短押し有効", 0, 0, 0);
    {
        int toggles = run_ms_count_toggles(true, 2000);
        bool ok = toggles == 0;
        printf("[%s] 押し続けても点滅しない: 反転 %d 回（期待 0）\n",
               ok ? "OK" : "NG", toggles);
        failures += !ok;
    }
    run_ms(false, 100);
    expect_leds("離しても短押しとして数えない", 0, 0, 0);
    short_press();
    expect_leds("短押し1回では変化なし", 0, 0, 0);
    short_press();
    expect_leds("短押し2回でLED1", 1, 0, 0);

    printf(failures ? "\n%d 件失敗\n" : "\nすべて成功\n", failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
