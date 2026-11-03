/*
 * LCD 两页界面：环境仪表盘和系统/OTA 状态页。
 *
 * 本模块只绘制调用者提供的数据快照，不直接访问传感器、UART 或 Flash。
 * 固定布局只在切页时绘制；动态数值只在变化时局部刷新，以减少 FSMC 写入
 * 和肉眼可见的整屏闪烁。所有函数都由同一个 UI 任务调用，因此页面状态无需锁。
 */
#include "app_ui.h"
#include "lcd.h"
#include "ota_port_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
    APP_UI_PAGE_ENVIRONMENT = 0, /* 温湿度、光照和空气质量四张卡片。 */
    APP_UI_PAGE_SYSTEM_STATUS   /* Wi-Fi、MQTT、OTA 进度和固件版本。 */
} AppUiPage_t;

#define APP_UI_HEADER_HEIGHT     52U
#define APP_UI_CARD_WIDTH        145U
#define APP_UI_CARD_HEIGHT       118U
#define APP_UI_CARD_LEFT_X       10U
#define APP_UI_CARD_RIGHT_X      165U
#define APP_UI_CARD_TOP_Y        72U
#define APP_UI_CARD_BOTTOM_Y     208U

/* 页面变量只由 LCD 任务访问，所以不需要额外的锁。 */
static AppUiPage_t g_current_page;            /* KEY1 当前选择的页面。 */
static bool g_page_needs_redraw;              /* true 时重画固定背景和边框。 */
static bool g_has_previous_environment;       /* 前一帧环境缓存是否可比较。 */
static EnvData_t g_previous_environment;      /* 用于跳过没有变化的 LCD 写入。 */
static bool g_has_previous_esp32_status;      /* 前一帧网关状态是否可比较。 */
static Esp32Status_t g_previous_esp32_status; /* 用于局部刷新网络/OTA 状态。 */

/*
 * 填充 LCD 的一个小矩形。
 * 底层 LCD 驱动没有单独的“擦除区域”函数，因此把这一小段重复操作封装起来。
 */
static void AppUi_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    uint32_t pixel_count;

    LCD_SetArea(x, y, width, height);
    LCD_WriteCmd(0x2CU);

    for (pixel_count = 0U; pixel_count < ((uint32_t)width * height); pixel_count++)
    {
        LCD_WriteData(color);
    }
}

/* LCD 当前字库稳定支持英文、数字和常用符号，因此界面字符串使用 ASCII。 */
static void AppUi_WriteText(uint16_t x, uint16_t y, uint16_t height, const char *text,
                            uint16_t foreground, uint16_t background)
{
    LCD_WriteAsciiString(x, y, height, (uint8_t *)text, foreground, background);
}

/* 画每一页顶部的蓝色标题条。 */
static void AppUi_DrawHeader(const char *title)
{
    AppUi_FillRect(0U, 0U, LCD_W, APP_UI_HEADER_HEIGHT, BLUE);
    AppUi_WriteText(12U, 14U, 24U, title, WHITE, BLUE);
}

/* 环境仪表盘右上角只显示最重要的网络结论。 */
static void AppUi_DrawDashboardStatus(const Esp32Status_t *status)
{
    const char *text = "ESP: WAITING";
    uint16_t color = YELLOW;

    if (status->received)
    {
        if (status->wifi_connected && status->mqtt_connected)
        {
            text = "ESP: ONLINE";
            color = GREEN;
        }
        else
        {
            text = "ESP: OFFLINE";
            color = RED;
        }
    }

    /* 先擦除旧状态，再显示新状态，防止短文本留下旧字符。 */
    AppUi_FillRect(180U, 14U, 132U, 24U, BLUE);
    AppUi_WriteText(184U, 19U, 12U, text, color, BLUE);
}

/* 两个页面都使用相同的灰色底部提示条，集中到这里可避免重复代码。 */
static void AppUi_DrawFooter(const char *text)
{
    AppUi_FillRect(0U, 438U, LCD_W, 42U, GRAY);
    AppUi_WriteText(16U, 451U, 16U, text, WHITE, GRAY);
}

/* 画数据卡片的边框和标签；数值刷新时不再重画这些固定内容。 */
static void AppUi_DrawCard(uint16_t x, uint16_t y, const char *label, uint16_t border_color)
{
    AppUi_FillRect(x, y, APP_UI_CARD_WIDTH, APP_UI_CARD_HEIGHT, WHITE);
    LCD_DrawRectangle(x, y, (uint16_t)(x + APP_UI_CARD_WIDTH - 1U),
                      (uint16_t)(y + APP_UI_CARD_HEIGHT - 1U), 2U, border_color);
    AppUi_WriteText((uint16_t)(x + 12U), (uint16_t)(y + 14U), 16U, label, BLACK, WHITE);
}

/* 擦除旧数值后再写入新数值，可避免整屏清除造成肉眼可见的闪烁。 */
static void AppUi_WriteCardValue(uint16_t x, uint16_t y, const char *value, uint16_t color)
{
    AppUi_FillRect((uint16_t)(x + 10U), (uint16_t)(y + 48U),
                   (uint16_t)(APP_UI_CARD_WIDTH - 20U), 32U, WHITE);
    AppUi_WriteText((uint16_t)(x + 12U), (uint16_t)(y + 52U), 24U, value, color, WHITE);
}

/* 四张卡片共用同一套有效/无数据显示规则，避免每项数据重复一个 else 分支。 */
static void AppUi_WriteValueOrNoData(uint16_t x, uint16_t y, bool valid,
                                     const char *value, uint16_t value_color)
{
    AppUi_WriteCardValue(x, y,
                         valid ? value : "NO DATA",
                         valid ? value_color : GRAY);
}

/* 环境数据页的固定布局，只在开机或切换回本页时绘制一次。 */
static void AppUi_DrawEnvironmentLayout(void)
{
    LCD_ClearAll(WHITE);
    AppUi_DrawHeader("ENV DASHBOARD");
    AppUi_DrawCard(APP_UI_CARD_LEFT_X, APP_UI_CARD_TOP_Y, "TEMPERATURE", RED);
    AppUi_DrawCard(APP_UI_CARD_RIGHT_X, APP_UI_CARD_TOP_Y, "HUMIDITY", GBLUE);
    AppUi_DrawCard(APP_UI_CARD_LEFT_X, APP_UI_CARD_BOTTOM_Y, "LIGHT", YELLOW);
    AppUi_DrawCard(APP_UI_CARD_RIGHT_X, APP_UI_CARD_BOTTOM_Y, "AIR QUALITY", GREEN);
    AppUi_DrawFooter("KEY1: SYSTEM STATUS");
}

/* 负温度要先取绝对值，才能正确显示 -1.2 C，而不是 -1.-2 C。 */
static void AppUi_FormatTemperature(int16_t temperature_tenth, char *text, size_t text_size)
{
    int32_t absolute_value = temperature_tenth;

    if (absolute_value < 0)
    {
        absolute_value = -absolute_value;
        (void)snprintf(text, text_size, "-%ld.%ld C", (long)(absolute_value / 10),
                       (long)(absolute_value % 10));
    }
    else
    {
        (void)snprintf(text, text_size, "%ld.%ld C", (long)(absolute_value / 10),
                       (long)(absolute_value % 10));
    }
}

/* 根据有效标志刷新四个传感器数值；没有接线时显示 NO DATA，而不是假数据。 */
static void AppUi_DrawEnvironmentValues(const EnvData_t *environment)
{
    char text[20];

    if (environment->temperature_valid)
    {
        AppUi_FormatTemperature(environment->temperature_tenth, text, sizeof(text));
    }
    AppUi_WriteValueOrNoData(APP_UI_CARD_LEFT_X, APP_UI_CARD_TOP_Y,
                             environment->temperature_valid, text, RED);

    if (environment->humidity_valid)
    {
        (void)snprintf(text, sizeof(text), "%u.%u %%", environment->humidity_tenth / 10U,
                       environment->humidity_tenth % 10U);
    }
    AppUi_WriteValueOrNoData(APP_UI_CARD_RIGHT_X, APP_UI_CARD_TOP_Y,
                             environment->humidity_valid, text, GBLUE);

    if (environment->light_valid)
    {
        (void)snprintf(text, sizeof(text), "%u lx", environment->light_lux);
    }
    AppUi_WriteValueOrNoData(APP_UI_CARD_LEFT_X, APP_UI_CARD_BOTTOM_Y,
                             environment->light_valid, text, BROWN);

    if (environment->air_valid)
    {
        (void)snprintf(text, sizeof(text), "%u ppm", environment->air_ppm);
    }
    AppUi_WriteValueOrNoData(APP_UI_CARD_RIGHT_X, APP_UI_CARD_BOTTOM_Y,
                             environment->air_valid, text, GREEN);
}

/*
 * OTA 页面现在只作入口预留：未来可新建 OTA 任务，并让它通过 UART 与 Arduino ESP32 通信。
 * 目前不执行联网、下载、校验或写 Flash，因此不会影响现有三任务运行。
 */
/* 绘制 OTA 下载进度条；进度值异常时按 100% 显示，避免越界。 */
static void AppUi_DrawProgressBar(uint8_t progress)
{
    uint16_t filled_width;

    if (progress > 100U)
    {
        progress = 100U;
    }

    LCD_DrawRectangle(40U, 350U, 280U, 374U, 2U, GREEN);
    AppUi_FillRect(42U, 352U, 236U, 20U, WHITE);

    filled_width = (uint16_t)((236U * progress) / 100U);
    if (filled_width > 0U)
    {
        AppUi_FillRect(42U, 352U, filled_width, 20U, GREEN);
    }
}

/* 系统状态页的固定边框和按键提示，只在进入页面时绘制一次。 */
static void AppUi_DrawSystemStatusPage(void)
{
    LCD_ClearAll(WHITE);
    AppUi_DrawHeader("SYSTEM STATUS");
    LCD_DrawRectangle(18U, 78U, 301U, 420U, 2U, GBLUE);
    AppUi_WriteText(40U, 94U, 16U, "ESP32 AND OTA STATUS", BLACK, WHITE);
    AppUi_DrawFooter("KEY1: ENV DASHBOARD");
}

/* 系统状态页只刷新会变化的区域。 */
static void AppUi_DrawSystemStatus(const Esp32Status_t *status)
{
    char text[40];

    AppUi_FillRect(30U, 120U, 270U, 295U, WHITE);

    if (!status->received)
    {
        AppUi_WriteText(40U, 130U, 16U, "ESP32  : WAITING", BROWN, WHITE);
        AppUi_WriteText(40U, 170U, 16U, "WIFI   : --", GRAY, WHITE);
        AppUi_WriteText(40U, 210U, 16U, "MQTT   : --", GRAY, WHITE);
        AppUi_WriteText(40U, 250U, 16U, "OTA    : --", GRAY, WHITE);
        AppUi_WriteText(40U, 400U, 16U, "VERSION: " OTA_CURRENT_VERSION, BLACK, WHITE);
        return;
    }

    AppUi_WriteText(40U, 130U, 16U, "ESP32  : CONNECTED", GREEN, WHITE);
    AppUi_WriteText(40U, 170U, 16U,
                    status->wifi_connected ? "WIFI   : OK" : "WIFI   : OFF",
                    status->wifi_connected ? GREEN : RED, WHITE);
    AppUi_WriteText(40U, 210U, 16U,
                    status->mqtt_connected ? "MQTT   : OK" : "MQTT   : OFF",
                    status->mqtt_connected ? GREEN : RED, WHITE);

    if (status->ota_active)
    {
        (void)snprintf(text, sizeof(text), "OTA    : %s %u%%", status->message,
                       status->ota_progress);
        AppUi_WriteText(40U, 250U, 16U, text, BROWN, WHITE);
        AppUi_DrawProgressBar(status->ota_progress);
    }
    else
    {
        (void)snprintf(text, sizeof(text), "OTA    : %s", status->message);
        AppUi_WriteText(40U, 250U, 16U, text, GREEN, WHITE);
    }

    AppUi_WriteText(40U, 400U, 16U, "VERSION: " OTA_CURRENT_VERSION, BLACK, WHITE);
}

void AppUi_Init(void)
{
    /* 第一次进入 UI 任务时请求绘制环境页。 */
    g_current_page = APP_UI_PAGE_ENVIRONMENT;
    g_page_needs_redraw = true;
    g_has_previous_environment = false;
    g_has_previous_esp32_status = false;
    (void)memset(&g_previous_environment, 0, sizeof(g_previous_environment));
    (void)memset(&g_previous_esp32_status, 0, sizeof(g_previous_esp32_status));
}

void AppUi_TogglePage(void)
{
    /* 一个按键只负责在两个页面之间来回切换，逻辑便于初学者理解。 */
    g_current_page = (g_current_page == APP_UI_PAGE_ENVIRONMENT) ?
                     APP_UI_PAGE_SYSTEM_STATUS : APP_UI_PAGE_ENVIRONMENT;
    g_page_needs_redraw = true;
}

void AppUi_Render(const EnvData_t *environment, const Esp32Status_t *esp32_status)
{
    bool esp32_status_changed;

    if ((environment == NULL) || (esp32_status == NULL))
    {
        return;
    }

    /* 两个输入都是调用者持有的局部快照，整个绘制过程中不会被其他任务改写。 */
    if (g_current_page == APP_UI_PAGE_ENVIRONMENT)
    {
        if (g_page_needs_redraw)
        {
            AppUi_DrawEnvironmentLayout();
        }

        /*
         * 两份快照都在写入前完整初始化，因此可以比较全部字节。
         * 数据没有变化就不操作 LCD，既减少闪烁，也给其他任务更多运行时间。
         */
        if (g_page_needs_redraw || !g_has_previous_environment ||
            (memcmp(&g_previous_environment, environment, sizeof(*environment)) != 0))
        {
            AppUi_DrawEnvironmentValues(environment);
            g_previous_environment = *environment;
            g_has_previous_environment = true;
        }

        /* 首页右上角的 ESP 摘要也要随串口状态变化而刷新。 */
        esp32_status_changed = !g_has_previous_esp32_status ||
                              (memcmp(&g_previous_esp32_status, esp32_status,
                                      sizeof(*esp32_status)) != 0);
        if (g_page_needs_redraw || esp32_status_changed)
        {
            AppUi_DrawDashboardStatus(esp32_status);
            g_previous_esp32_status = *esp32_status;
            g_has_previous_esp32_status = true;
        }
    }
    else
    {
        if (g_page_needs_redraw)
        {
            AppUi_DrawSystemStatusPage();
        }

        if (g_page_needs_redraw || !g_has_previous_esp32_status ||
            (memcmp(&g_previous_esp32_status, esp32_status, sizeof(*esp32_status)) != 0))
        {
            AppUi_DrawSystemStatus(esp32_status);
            g_previous_esp32_status = *esp32_status;
            g_has_previous_esp32_status = true;
        }
    }

    g_page_needs_redraw = false;
}
