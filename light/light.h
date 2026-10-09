#ifndef LIGHT_H
#define LIGHT_H

#include <stdbool.h>
#include <stdint.h>

#define LIGHT_LED_NUM 3

/* ---- ハードウェア依存部（使用するマイコンに合わせて実装する） ---- */
/* スイッチの生の入力状態を返す（押されていれば true） */
bool hal_switch_is_on(void);
/* LED を点灯/消灯する（index: 0〜LIGHT_LED_NUM-1） */
void hal_led_write(uint8_t index, bool on);

/* ---- ライト制御 ---- */
/* 起動時に1回呼ぶ（全LED消灯） */
void light_init(void);
/* 10msec周期のタイマ割り込みから呼ぶ */
void light_tick_10ms(void);

#endif /* LIGHT_H */
