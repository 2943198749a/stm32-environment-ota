/*
 * LCD 两页界面对外接口。
 *
 * 本模块只负责显示，调用者负责提供已经复制好的环境和 ESP32 状态快照。
 * LCD 硬件必须先由 main.c 初始化；以下接口只能由 UI 任务调用，不能在中断中调用。
 */
#ifndef __APP_UI_H
#define __APP_UI_H

#include "env_sensor.h"
#include "esp32_status.h"

/* 初始化页面缓存并请求首次整页绘制，不访问 LCD 寄存器。 */
void AppUi_Init(void);

/* KEY1 产生一次有效按下事件时调用，在环境页和系统状态页之间切换。 */
void AppUi_TogglePage(void);

/*
 * 刷新当前页面；两个指针都不能为 NULL，数据所有权仍属于调用者。
 * 函数会缓存结构体副本，并只重绘发生变化的区域。LCD 写入可能耗时，
 * 所以调用者不能在临界区或中断服务函数中调用。
 */
void AppUi_Render(const EnvData_t *environment, const Esp32Status_t *esp32_status);

#endif
