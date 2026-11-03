/*
 * LCD 控制器寄存器初始化与基础绘图算法。
 *
 * 这里直接操作 FSMC 映射的命令/数据地址；上层 app_ui.c 只组合这些原语。
 * 寄存器值、延时和绘图算法与现有硬件匹配，本轮重构只补充接口说明，
 * 不改变控制器时序和像素输出。
 *
 * 推荐阅读顺序：先看 WriteCmd/WriteData 理解“命令口”和“数据口”，再看
 * SetArea + ClearAll 理解显存窗口，最后看字符点阵和几何图形。RegConfig 中的
 * 常量来自 LCD 控制器初始化序列，除非同时核对屏幕数据手册，否则不要改动。
 */
#include "lcd.h"
#include "lcd_font.h"

// 1. 基本控制操作
// 1.1 初始化
void LCD_Init(void)
{
    /* FSMC 建立内存映射总线，后续写指针就等价于向 LCD 发送并行总线数据。 */
    MX_FSMC_Init();

    /* 硬件复位保证控制器处于已知状态，然后点背光、写寄存器初始化序列。 */
    LCD_Reset();

    LCD_BGOn();

    LCD_RegConfig();
}

// 1.1.1 复位
void LCD_Reset(void)
{
    // 直接将PG15拉低
    GPIOG->ODR &= ~GPIO_ODR_ODR15;

    // 稍作延迟，然后拉高
    HAL_Delay(100);
    GPIOG->ODR |= GPIO_ODR_ODR15;

    HAL_Delay(100);
}

// 1.1.2 开关背光
void LCD_BGOn(void)
{
    // PB0输出高电平，点亮背光LED
    GPIOB->ODR |= GPIO_ODR_ODR0;
}
void LCD_BGOff(void)
{
    // PB0输出低电平，关闭背光LED
    GPIOB->ODR &= ~GPIO_ODR_ODR0;
}

// 1.1.3 初始化LCD寄存器
void LCD_RegConfig(void)
{
    /* 0xE0：正伽马曲线。后续 15 个参数共同决定灰阶和颜色过渡。 */
    LCD_WriteCmd(0xE0);
    LCD_WriteData(0x00);
    LCD_WriteData(0x07);
    LCD_WriteData(0x10);
    LCD_WriteData(0x09);
    LCD_WriteData(0x17);
    LCD_WriteData(0x0B);
    LCD_WriteData(0x41);
    LCD_WriteData(0x89);
    LCD_WriteData(0x4B);
    LCD_WriteData(0x0A);
    LCD_WriteData(0x0C);
    LCD_WriteData(0x0E);
    LCD_WriteData(0x18);
    LCD_WriteData(0x1B);
    LCD_WriteData(0x0F);

    /* 2. 设置灰阶电压以调整TFT面板的伽马特性，负校准 */
    LCD_WriteCmd(0XE1);
    LCD_WriteData(0x00);
    LCD_WriteData(0x17);
    LCD_WriteData(0x1A);
    LCD_WriteData(0x04);
    LCD_WriteData(0x0E);
    LCD_WriteData(0x06);
    LCD_WriteData(0x2F);
    LCD_WriteData(0x45);
    LCD_WriteData(0x43);
    LCD_WriteData(0x02);
    LCD_WriteData(0x0A);
    LCD_WriteData(0x09);
    LCD_WriteData(0x32);
    LCD_WriteData(0x36);
    LCD_WriteData(0x0F);

    /* 3.  Adjust Control 3 (F7h)  */
    /*LCD_WriteCmd(0XF7);
   LCD_WriteData(0xA9);
   LCD_WriteData(0x51);
   LCD_WriteData(0x2C);
   LCD_WriteData(0x82);*/
    /* DSI write DCS command, use loose packet RGB 666 */

    /* 4. 电源控制1*/
    LCD_WriteCmd(0xC0);
    LCD_WriteData(0x11); /* 正伽马电压 */
    LCD_WriteData(0x09); /* 负伽马电压 */

    /* 5. 电源控制2 */
    LCD_WriteCmd(0xC1);
    LCD_WriteData(0x02);
    LCD_WriteData(0x03);

    /* 6. VCOM控制 */
    LCD_WriteCmd(0XC5);
    LCD_WriteData(0x00);
    LCD_WriteData(0x0A);
    LCD_WriteData(0x80);

    /* 7. Frame Rate Control (In Normal Mode/Full Colors) (B1h) */
    LCD_WriteCmd(0xB1);
    LCD_WriteData(0xB0);
    LCD_WriteData(0x11);

    /* 8.  Display Inversion Control (B4h) （正负电压反转，减少电磁干扰）*/
    LCD_WriteCmd(0xB4);
    LCD_WriteData(0x02);

    /* 9.  Display Function Control (B6h)  */
    LCD_WriteCmd(0xB6);
    LCD_WriteData(0x0A);
    LCD_WriteData(0xA2);

    /* 10. Entry Mode Set (B7h)  */
    LCD_WriteCmd(0xB7);
    LCD_WriteData(0xc6);

    /* 11. HS Lanes Control (BEh) */
    LCD_WriteCmd(0xBE);
    LCD_WriteData(0x00);
    LCD_WriteData(0x04);

    /* 12.  Interface Pixel Format (3Ah) */
    LCD_WriteCmd(0x3A);
    LCD_WriteData(0x55); /* 0x55 : 16 bits/pixel  */

    /* 13. Sleep Out (11h) 关闭休眠模式 */
    LCD_WriteCmd(0x11);

    /* 14. 设置屏幕方向和RGB */
    LCD_WriteCmd(0x36);
    LCD_WriteData(0x08);

    HAL_Delay(120);

    /* 14. display on */
    LCD_WriteCmd(0x29);
}

// 1.2 写命令（发出一个指令）
void LCD_WriteCmd(uint16_t cmd)
{
    /* 地址线选择“命令寄存器”；一次 volatile 外设写由 FSMC 产生总线时序。 */
    *LCD_ADDR_CMD = cmd;
}

// 1.3 写数据（发出一个数据）
void LCD_WriteData(uint16_t data)
{
    /* 地址线选择“数据寄存器”；含义由最近一次 LCD_WriteCmd 决定。 */
    *LCD_ADDR_DATA = data;
}

// 1.4 读数据
uint16_t LCD_ReadData(void)
{
    /* 同一数据地址也可读，FSMC 会自动产生 LCD 读时序。 */
    return *LCD_ADDR_DATA;
}

// 2. 具体的命令操作

// 2.1 返回ID信息
uint32_t LCD_ReadID(void)
{
    // 首先发送读取ID的命令
    LCD_WriteCmd(0x04);

    // 定义变量，接收三字节的ID信息
    uint32_t id = 0;

    /* 第一次读通常是控制器要求的 dummy read，只触发时序，不计入 ID。 */
    LCD_ReadData();

    // 依次读取三个字节的数据，放置在id的对应位置
    id |= (LCD_ReadData() & 0xff) << 16;
    id |= (LCD_ReadData() & 0xff) << 8;
    id |= (LCD_ReadData() & 0xff);

    return id;
}

// 2.2 设置区域范围，给定起始点的坐标（行列号），以及区域的宽和高
void LCD_SetArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    /*
     * 设置后控制器只在这个矩形窗口内递增显存地址。调用者还要发送 0x2C，
     * 然后连续写 w*h 个 RGB565 像素。w/h 必须非零且区域不能超出屏幕。
     */
    // 1. 设置列范围
    LCD_WriteCmd(0x2a);

    // 1.1 设置开始列
    LCD_WriteData(x >> 8 & 0xff);
    LCD_WriteData(x & 0xff);

    // 1.2 设置结束列
    LCD_WriteData((x + w - 1) >> 8 & 0xff);
    LCD_WriteData((x + w - 1) & 0xff);

    // 2. 设置行范围
    LCD_WriteCmd(0x2b);

    // 2.1 设置开始行
    LCD_WriteData(y >> 8 & 0xff);
    LCD_WriteData(y & 0xff);

    // 2.2 设置结束行
    LCD_WriteData((y + h - 1) >> 8 & 0xff);
    LCD_WriteData((y + h - 1) & 0xff);
}

// 2.3 清屏
void LCD_ClearAll(uint16_t color)
{
    // 1. 设置区域范围为全屏
    LCD_SetArea(0, 0, LCD_W, LCD_H);

    // 2. 向对应区域范围发送数据
    // 2.1 发送命令
    LCD_WriteCmd(0x2c);

    // 2.2 用循环遍历所有像素点，依次写入数据
    for (uint32_t i = 0; i < LCD_W * LCD_H; i++)
    {
        LCD_WriteData(color);
    }
}

// 3. 上层操作接口
// 3.1 显示一个ASCII码字符
void LCD_WriteAsciiChar(uint16_t x, uint16_t y, uint16_t height, uint8_t c, uint16_t fColor, uint16_t bColor)
{
    // 1. 先确定区域
    LCD_SetArea(x, y, height / 2, height);

    // 2. 发送写入显存指令
    LCD_WriteCmd(0x2C);

    // 3. 遍历整个区域所有的像素点，判断是否为字符的笔画
    // 按照字体大小，到不同的数组中寻找字符对应的点阵

    // 先计算当前字符c在字库中数组的索引位置
    /* 字库从空格开始排列，因此 ASCII 码减去 ' ' 得到点阵数组下标。 */
    uint8_t index = c - ' ';

    // 3.1 如果字体是1608或者1206，用一个字节表示一行点阵
    if (height == 16 || height == 12)
    {
        // 遍历每一行，每一列（双重循环）
        for (uint8_t i = 0; i < height; i++)
        {
            // 根据字体大小，在字库中选择当前行对应的第i个字节，这就是第i行的点阵
            uint8_t tempByte = (height == 16) ? ascii_1608[index][i] : ascii_1206[index][i];

            //  遍历当前行中的每一列（每个像素点 - 字节中的每一位）
            for (uint8_t j = 0; j < height / 2; j++)
            {
                // 每次只取最低位
                if (tempByte & 0x01)
                {
                    // 如果是1，显示字体颜色
                    LCD_WriteData(fColor);
                }
                else
                {
                    // 如果是0，显示背景颜色
                    LCD_WriteData(bColor);
                }
                // 右移一位
                tempByte >>= 1;
            }
        }
    }

    // 3.2 如果字体是2412，用两个字节表示一行点阵
    else if (height == 24)
    {
        // 遍历每一行，每一列（双重循环）
        // i 为字节编号
        for (uint8_t i = 0; i < height * 2; i++)
        {
            // 根据字体大小，在字库中选择当前行对应的第i个字节
            uint8_t tempByte = ascii_2412[index][i];

            // 根据当前i的值，判断字节中遍历多少位数据（奇数的话只遍历低4位）
            uint8_t jCount = (i % 2) ? 4 : 8;

            //  遍历当前字节中的每一列（每个像素点 - 字节中的每一位）
            for (uint8_t j = 0; j < jCount; j++)
            {
                // 每次只取最低位
                if (tempByte & 0x01)
                {
                    // 如果是1，显示字体颜色
                    LCD_WriteData(fColor);
                }
                else
                {
                    // 如果是0，显示背景颜色
                    LCD_WriteData(bColor);
                }
                // 右移一位
                tempByte >>= 1;
            }
        }
    }

    // 3.3 如果字体是3216，用两个字节表示一行点阵
    else if (height == 32)
    {
        // 遍历每一行，每一列（双重循环）
        for (uint8_t i = 0; i < height * 2; i++)
        {
            // 根据字体大小，在字库中选择当前行对应的第i个字节，这就是第i行的点阵
            uint8_t tempByte = ascii_3216[index][i];

            //  遍历当前字节中的每一列（每个像素点 - 字节中的每一位）
            for (uint8_t j = 0; j < 8; j++)
            {
                // 每次只取最低位
                if (tempByte & 0x01)
                {
                    // 如果是1，显示字体颜色
                    LCD_WriteData(fColor);
                }
                else
                {
                    // 如果是0，显示背景颜色
                    LCD_WriteData(bColor);
                }
                // 右移一位
                tempByte >>= 1;
            }
        }
    }
}

// 3.2 显示字符串
void LCD_WriteAsciiString(uint16_t x, uint16_t y, uint16_t height, uint8_t *str, uint16_t fColor, uint16_t bColor)
{
    /* 本函数不复制字符串，调用期间 str 必须有效并以 '\0' 结尾。 */
    // 定义一个变量，指明当前遍历的字符
    uint8_t i = 0;

    // 利用\0标志判断字符串是否结束
    while (str[i] != '\0')
    {
        // 判断是否遇到\n需要换行，如果没遇到，就直接增加x，继续显示
        if (str[i] != '\n')
        {
            // 另外判断一下是否到达当前行的末尾，如果要超出边界就自动换行
            if (x + height / 2 > LCD_W)
            {
                // 换行
                x = 0;
                y += height;
            }

            LCD_WriteAsciiChar(x, y, height, str[i], fColor, bColor);
            x += height / 2;
        }
        else
        {
            // 换行
            x = 0;
            y += height;
        }
        i++;
    }
}

// 3.3 显示一个中文字符
void LCD_WriteChineseChar(uint16_t x, uint16_t y, uint16_t height, uint8_t index, uint16_t fColor, uint16_t bColor)
{
    /* 当前 chinese 字库按 32x32、每字 128 字节组织，height 实际应传 32。 */
    // 设置显示区域
    LCD_SetArea(x, y, height, height);

    // 发送指令
    LCD_WriteCmd(0x2C);

    // 发送数据
    // 遍历每一行，每一列（双重循环），要求传入的height必须是 32
    for (uint8_t i = 0; i < 128; i++)
    {
        // 根据字体大小，在字库中选择当前行对应的第i个字节，这就是第i行的点阵
        uint8_t tempByte = chinese[index][i];

        //  遍历当前字节中的每一列（每个像素点 - 字节中的每一位）
        for (uint8_t j = 0; j < 8; j++)
        {
            // 每次只取最低位
            if (tempByte & 0x01)
            {
                // 如果是1，显示字体颜色
                LCD_WriteData(fColor);
            }
            else
            {
                // 如果是0，显示背景颜色
                LCD_WriteData(bColor);
            }
            // 右移一位
            tempByte >>= 1;
        }
    }
}

// 3.4 显示图片
void LCD_DisplayAtguiguLogo(uint16_t x, uint16_t y)
{
    /* 图片尺寸固定为 227x68，数组中每两个小端字节拼成一个 RGB565 像素。 */
    // 设置显示区域
    LCD_SetArea(x, y, 227, 68);

    // 发送指令
    LCD_WriteCmd(0x2C);

    uint16_t len = sizeof(gImage_logo);

    // 每次取两个字节，拼接成一个像素点的RGB数据
    for (uint16_t i = 0; i < len; i += 2)
    {
        // 16位表示一个像素点，低位在前，高位在后
        uint16_t p = gImage_logo[i] + (gImage_logo[i + 1] << 8);
        LCD_WriteData(p);
    }
}

// 3.5 画出一个点，给定左上角起始点的坐标，以及方点的宽度
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    // 1. 设置区域
    LCD_SetArea(x, y, w, w);

    // 2. 发送命令
    LCD_WriteCmd(0x2C);

    // 3. 将区域内所有像素点涂上给定的颜色
    for (uint16_t i = 0; i < w * w; i++)
    {
        LCD_WriteData(color);
    }
}

// 3.6 画出一条线，给定起点和终点的坐标
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t w, uint16_t color)
{
    /* 当前简单实现要求 x1<=x2；竖线分支还要求 y1<=y2。 */
    // 计算直线方程中的k和b
    // y = kx + b, k = (y1-y2)/(x1-x2), b = y1 - k * x1

    // 先考虑x1 == x2 的情况，一条竖线
    if (x1 == x2)
    {
        for (uint16_t y = y1; y <= y2; y++)
        {
            LCD_DrawPoint(x1, y, w, color);
        }
        return;
    }

    // 先计算k 和 b
    double k = 1.0 * (y1 - y2) / (x1 - x2);
    double b = y1 - k * x1;

    // 根据直线方程，依次画点（默认要求x1 <= x2）
    for (uint16_t x = x1; x <= x2; x++)
    {
        uint16_t y = (uint16_t)(k * x + b);
        LCD_DrawPoint(x, y, w, color);
    }
}

// 3.7 画出一个长方形
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t w, uint16_t color)
{
    LCD_DrawLine(x1, y1, x2, y1, w, color);
    LCD_DrawLine(x2, y1, x2, y2, w, color);
    LCD_DrawLine(x1, y1, x1, y2, w, color);
    LCD_DrawLine(x1, y2, x2, y2, w, color);
}

// 3.8 画圆，给定圆心坐标以及半径
void LCD_DrawCircle(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t color)
{
    /* 教学版实现逐角度调用 sin/cos，直观但比整数中点画圆算法慢。 */
    // 按照极坐标方程，画出每一个点
    // x = x0 + rcosθ
    // y = y0 + rsinθ
    for (uint16_t theta = 0; theta < 360; theta++)
    {
        uint16_t x = xCenter + r * cos(3.14 * theta / 180);
        uint16_t y = yCenter + r * sin(3.14 * theta / 180);

        LCD_DrawPoint(x, y, w, color);
    }
}

// 画圆优化实现：同时画出4个象限的点
void LCD_DrawCircle_Pro(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t color)
{
    /* 只计算 0~90 度，再利用圆的四象限对称性补齐其余三个点。 */
    // 按照极坐标方程，画出每一个点
    // x = x0 + rcosθ
    // y = y0 + rsinθ
    for (uint16_t theta = 0; theta <= 90; theta++)
    {
        uint16_t delta_x = r * cos(3.14 * theta / 180);
        uint16_t delta_y = r * sin(3.14 * theta / 180);

        // 第一象限
        uint16_t x = xCenter + delta_x;
        uint16_t y = yCenter + delta_y;

        LCD_DrawPoint(x, y, w, color);

        // 第二象限
        x = xCenter - delta_x;
        y = yCenter + delta_y;

        LCD_DrawPoint(x, y, w, color);

        // 第三象限
        x = xCenter - delta_x;
        y = yCenter - delta_y;

        LCD_DrawPoint(x, y, w, color);

        // 第四象限
        x = xCenter + delta_x;
        y = yCenter - delta_y;

        LCD_DrawPoint(x, y, w, color);
    }
}

// 3.9 画实心圆
void LCD_DrawFilledCircle(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t bColor, uint16_t fColor)
{
    /* 从半径 0 画到 r：内部用 bColor，最外圈用 fColor。 */
    // 对r取不同的值，依次画空心圆
    for (uint16_t i = 0; i <= r; i++)
    {
        for (uint16_t theta = 0; theta < 360; theta++)
        {
            uint16_t x = xCenter + i * cos(3.14 * theta / 180);
            uint16_t y = yCenter + i * sin(3.14 * theta / 180);

            if (i == r)
            {
                LCD_DrawPoint(x, y, w, fColor);
            }
            else
            {
                LCD_DrawPoint(x, y, w, bColor);
            }
        }
    }
}

// 实心圆优化
void LCD_DrawFilledCircle_Pro(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t bColor, uint16_t fColor)
{
    /* 对称计算上下边界点，再用水平线填满圆内部，减少重复三角函数计算。 */
    // 按照四个象限找到4个点，直接画出两条线
        for (uint16_t theta = 0; theta <= 90; theta++)
    {
        uint16_t delta_x = r * cos(3.14 * theta / 180);
        uint16_t delta_y = r * sin(3.14 * theta / 180);

        // 第一象限
        uint16_t x1 = xCenter + delta_x;
        uint16_t y1 = yCenter + delta_y;

        LCD_DrawPoint(x1, y1, w, fColor);

        // 第二象限
        uint16_t x2 = xCenter - delta_x;
        uint16_t y2 = yCenter + delta_y;

        LCD_DrawPoint(x2, y2, w, fColor);

        // 横向两点连接一条直线
        LCD_DrawLine(x2 + w, y2, x1 - w, y1, w, bColor);

        // 第三象限
        x2 = xCenter - delta_x;
        y2 = yCenter - delta_y;

        LCD_DrawPoint(x2, y2, w, fColor);

        // 第四象限
        x1 = xCenter + delta_x;
        y1 = yCenter - delta_y;

        LCD_DrawPoint(x1, y1, w, fColor);

        // 横向两点连接一条直线
        LCD_DrawLine(x2 + w, y2, x1 - w, y1, w, bColor);
    }
}
