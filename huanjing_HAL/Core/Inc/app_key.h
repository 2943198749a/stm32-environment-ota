/*
 * 板载 KEY1 输入与消抖接口。
 * 模块只产生“一次按下”事件，不负责切换页面；GPIO 时钟和输入模式由 CubeMX 配置。
 */
#ifndef __APP_KEY_H
#define __APP_KEY_H

/* GPIOF、GPIO_PIN_11 等按键引脚宏。 */
#include "main.h"
/* bool、true 和 false 的定义。 */
#include <stdbool.h>

/* 原理图第 8 页确认：KEY1 接在 PF8（KEY4 对应 PF11）。 */
#define APP_KEY1_PORT GPIOF
#define APP_KEY1_PIN  GPIO_PIN_8

/* 初始化 KEY1(PF8) 的软件状态；应在 UI 任务启动时调用一次，不会阻塞。 */
void AppKey_Init(void);
/*
 * 轮询 KEY1 并执行 30 ms 消抖；一次按下到松开只返回一次 true。
 * 必须在任务中周期调用，不能在 GPIO 中断中调用。
 */
bool AppKey_PageTogglePressed(void);

#endif
