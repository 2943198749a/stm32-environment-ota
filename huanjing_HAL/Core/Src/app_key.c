/*
 * 板载 KEY1 消抖与按下事件实现。
 * 当前算法属于“变化后锁定 30 ms”：确认一次稳定变化后，短时间内忽略后续
 * 抖动；只有状态从“松开”变成“按下”时返回一次 true，长按不会反复切页。
 */
/* bool 类型和本文件的函数声明。 */
#include "app_key.h"

/* 按键机械抖动通常只持续几毫秒；30 ms 可以过滤抖动又不会感觉按键迟钝。 */
#define APP_KEY_DEBOUNCE_MS  30U

/* 记录按键上一次稳定的逻辑电平。 */
static GPIO_PinState g_stable_state;
/* 记录原始电平上次发生变化的时刻。 */
static uint32_t g_last_change_ms;

/* 初始化 KEY1 的按键状态，PF8 由 gpio.c 配为上拉输入。 */
void AppKey_Init(void)
{
    /* 读取 PF8 的当前状态：未按下为高电平，按下为低电平。 */
    g_stable_state = HAL_GPIO_ReadPin(APP_KEY1_PORT, APP_KEY1_PIN);
    /* 记录初始化时刻，消抖计时从这里开始。 */
    g_last_change_ms = HAL_GetTick();
}

/* 在 KEY1 低电平按下且稳定超过 APP_KEY_DEBOUNCE_MS 时，只返回一次 true。 */
bool AppKey_PageTogglePressed(void)
{
    /* raw_state 是直接从引脚读到的电平，可能包含按键机械抖动。 */
    GPIO_PinState raw_state = HAL_GPIO_ReadPin(APP_KEY1_PORT, APP_KEY1_PIN);
    /* now 是当前毫秒数，用来计算已经稳定了多久。 */
    uint32_t now = HAL_GetTick();

    /* 引脚电平没有变化，说明没有新的按下或松开动作。 */
    if (raw_state == g_stable_state)
    {
        return false;
    }

    /* 距上次确认变化不足 30 ms 时忽略本次采样，用锁定窗口过滤机械抖动。 */
    if ((now - g_last_change_ms) < APP_KEY_DEBOUNCE_MS)
    {
        return false;
    }

    /* 锁定窗口结束后若采样与稳定值不同，就接受为一次新的稳定变化。 */
    g_stable_state = raw_state;
    /* 保存本次确认变化的时间。 */
    g_last_change_ms = now;
    /* KEY1 为低电平有效：按下时返回 true，松开时返回 false。 */
    return (g_stable_state == GPIO_PIN_RESET);
}
