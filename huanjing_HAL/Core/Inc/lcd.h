/*
 * 320×480 TFT LCD 的 FSMC 并行总线驱动。
 *
 * STM32 通过两个内存映射地址分别写 LCD 命令和像素数据；颜色统一使用
 * RGB565。该驱动不负责页面状态或业务数据，页面布局由 app_ui.c 管理。
 * 绘制会同步写入大量像素，只能在任务上下文使用，不能在中断或持有
 * FreeRTOS 临界区时调用。
 */
#ifndef __LCD_H
#define __LCD_H

#include "fsmc.h"
#include <math.h>

/*
 * FSMC Bank1 NOR/SRAM4 映射到 0x6C000000。这里借助一根地址线区分命令和
 * 数据：写 LCD_ADDR_CMD 时 RS/DC 为命令态，写 LCD_ADDR_DATA 时为数据态。
 * 这些地址表示外设而非普通 RAM；以后重构宏类型时应考虑使用 volatile。
 */
#define SRAM_BANK1_4 0x6C000000
#define LCD_ADDR_CMD (uint16_t *)SRAM_BANK1_4
#define LCD_ADDR_DATA (uint16_t *)(SRAM_BANK1_4 + (1 << 11))
// #define LCD_ADDR_DATA (uint16_t *)(0x6cfffffe)

// 定义显示屏的宽和高
#define LCD_W 320
#define LCD_H 480

/* 常见颜色 */
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0XBC40 // 棕色
#define BRRED 0XFC07 // 棕红色
#define GRAY 0X8430  // 灰色

/* 复位控制器、配置寄存器并打开背光；FSMC 和 GPIO 必须已经初始化。 */
void LCD_Init(void);

/* 通过 PG15 给 LCD 控制器产生低脉冲硬件复位。 */
void LCD_Reset(void);

/* 通过 PB0 打开或关闭背光；不会改变 LCD 显存内容。 */
void LCD_BGOn(void);
void LCD_BGOff(void);

/* 写入屏幕控制器的伽马、电源、像素格式、方向和开显示寄存器序列。 */
void LCD_RegConfig(void);

/* 向内存映射的 LCD 命令口写一个 16 位命令。 */
void LCD_WriteCmd(uint16_t cmd);

/* 向数据口写一个参数或 RGB565 像素，含义取决于前一个命令。 */
void LCD_WriteData(uint16_t data);

/* 从数据口读一个 16 位值；部分命令要求先做一次无效读取。 */
uint16_t LCD_ReadData(void);

// 2. 具体的命令操作

/* 发送 0x04 命令并读取控制器的三字节 ID。 */
uint32_t LCD_ReadID(void);

/* 用一个 RGB565 颜色覆盖 320×480 全屏；调用期间会持续占用 FSMC。 */
void LCD_ClearAll(uint16_t color);
/*
 * 设置后续像素写入窗口。
 * x/y 是左上角像素坐标，w/h 是像素宽高；调用者必须保证区域不超出屏幕。
 */
void LCD_SetArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

/* 绘制一个 ASCII 字符；height 支持 12、16、24 或 32。 */
void LCD_WriteAsciiChar(uint16_t x, uint16_t y, uint16_t height, uint8_t c,
                        uint16_t fColor, uint16_t bColor);

/*
 * 在 x/y 位置绘制以 '\0' 结尾的 ASCII 字符串。
 * height 选择字库高度，fColor/bColor 分别是 RGB565 前景色和背景色。
 */
void LCD_WriteAsciiString(uint16_t x, uint16_t y, uint16_t height, uint8_t *str,
                          uint16_t fColor, uint16_t bColor);

/* 按 chinese 点阵数组下标绘制一个 32x32 中文字符。 */
void LCD_WriteChineseChar(uint16_t x, uint16_t y, uint16_t height, uint8_t index,
                          uint16_t fColor, uint16_t bColor);

/* 在指定左上角绘制字库中固定的 227x68 RGB565 Logo。 */
void LCD_DisplayAtguiguLogo(uint16_t x, uint16_t y);

/* 用颜色填充一个 w x w 的方点；较大的 w 可充当画笔宽度。 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t w, uint16_t color);

/* 用直线方程逐点绘制；当前实现要求 x1<=x2，竖线还要求 y1<=y2。 */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t w, uint16_t color);

/*
 * 绘制矩形边框；x1/y1 与 x2/y2 是两个对角像素，w 是边框像素宽度，
 * color 是 RGB565。坐标必须位于屏幕范围内。
 */
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                       uint16_t w, uint16_t color);

/* 基础版计算 360 个角度；Pro 版利用四象限对称性减少三角函数次数。 */
void LCD_DrawCircle(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t color);
void LCD_DrawCircle_Pro(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t color);

/* bColor 填内部、fColor 画边界；Pro 版通过水平线填充减少重复画点。 */
void LCD_DrawFilledCircle(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t bColor, uint16_t fColor);
void LCD_DrawFilledCircle_Pro(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t bColor, uint16_t fColor);
#endif

