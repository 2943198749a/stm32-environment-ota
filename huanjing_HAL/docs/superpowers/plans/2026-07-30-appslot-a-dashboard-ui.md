# AppSlotA Dashboard UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 AppSlotA 的 LCD 默认页改为清晰的环境监测仪表盘，并由 KEY1 切换到 ESP32 与 OTA 状态页。

**Architecture:** 只调整 `app_ui.c` 的绘制函数，不更改 FreeRTOS 任务或 CubeMX 自动生成代码。现有 `AppUi_Render(const EnvData_t *, const Esp32Status_t *)` 接口继续接收数据；页面内部根据当前页分别绘制环境卡片或系统状态，并仅在数据变化时刷新动态区域。

**Tech Stack:** STM32F103、HAL、FreeRTOS CMSIS-RTOS v2、现有 `lcd.c` 驱动、Keil MDK AppSlotA Target。

## Global Constraints

- 仅修改 `Core/Src/app_ui.c`；不改动 CubeMX 生成区、`freertos.c`、传感器、ESP32 通信和 OTA 协议。
- LCD 字库只稳定支持英文、数字和常用 ASCII 符号；界面显示文本必须使用 ASCII。
- 继续使用 `EnvData_t` 和 `Esp32Status_t`，无效数据必须显示 `NO DATA`。
- 版本号只从 `OTA_CURRENT_VERSION` 读取，不在 UI 内重复定义。
- 此工程没有可运行的主机单元测试框架，且绘制函数直接驱动 LCD；验证采用 Keil 编译和实机显示验收，不新增与硬件脱离的伪测试。
- 当前工程目录没有 Git 仓库；每个“提交”检查点记录为完成状态，不执行 `git commit`。

---

## File Structure

- Modify: `Core/Src/app_ui.c`
  - 保留页面切换、缓存比较和四个传感器卡片逻辑。
  - 新增仪表盘页头连接状态、系统状态页布局、OTA 进度条和版本号显示。
- No new files.

### Task 1: 完成环境仪表盘固定布局

**Files:**
- Modify: `Core/Src/app_ui.c` 中 `AppUi_DrawHeader`、`AppUi_DrawEnvironmentLayout`，并新增私有状态摘要绘制函数。
- Test: Keil `AppSlotA` 编译，以及实机环境页验收。

**Interfaces:**
- Consumes: `const Esp32Status_t *esp32_status` 中的 `received`、`wifi_connected`、`mqtt_connected`。
- Produces: 私有函数 `AppUi_DrawDashboardStatus(const Esp32Status_t *status)`，由环境页绘制流程调用。

- [ ] **Step 1: 写出环境页的失败验收条件**

在开发板运行当前 AppSlotA，记录以下现状：页头只显示 `SMART ENVIRONMENT`，无法从首页识别 ESP32 是否在线。该现象就是本任务修改前应失败的验收结果。

- [ ] **Step 2: 确认失败结果**

在 Keil 中选择 `AppSlotA`，下载并复位。预期观察到旧标题 `SMART ENVIRONMENT`，且页头右侧没有 `ESP: WAITING`、`ESP: ONLINE` 或 `ESP: OFFLINE`。

- [ ] **Step 3: 最小化实现仪表盘页头与布局**

在 `app_ui.c` 中实现如下私有函数；状态文字只使用现有 ASCII 字库字符：

```c
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

    AppUi_FillRect(175U, 14U, 140U, 24U, BLUE);
    AppUi_WriteText(178U, 18U, 12U, text, color, BLUE);
}
```

将环境页标题改为 `ENV DASHBOARD`，页尾改为 `KEY1: SYSTEM STATUS`。在 `AppUi_Render()` 的环境页分支中，固定布局首次绘制后调用 `AppUi_DrawDashboardStatus(esp32_status)`；当 ESP32 状态缓存变化时也重新绘制这个小区域，不重绘四张传感器卡片。

- [ ] **Step 4: 确认环境页通过验收**

重新编译 Keil `AppSlotA`。预期无新增错误和警告。下载后，默认页标题为 `ENV DASHBOARD`；页头右侧会按状态显示 `ESP: WAITING`、`ESP: ONLINE` 或 `ESP: OFFLINE`；温度、湿度、光照、空气质量仍显示原有数值或 `NO DATA`。

- [ ] **Step 5: 记录检查点**

确认仅修改 `Core/Src/app_ui.c`。由于当前目录无 Git 仓库，不执行提交。

### Task 2: 将 OTA 预留页替换为系统状态页

**Files:**
- Modify: `Core/Src/app_ui.c` 中 `AppUi_DrawOtaReservedPage`、`AppUi_DrawOtaStatus`。
- Test: Keil `AppSlotA` 编译，以及 KEY1 切页和 OTA 状态验收。

**Interfaces:**
- Consumes: `Esp32Status_t` 的 `received`、`wifi_connected`、`mqtt_connected`、`ota_active`、`ota_progress`、`message`；`OTA_CURRENT_VERSION` 宏。
- Produces: 私有函数 `AppUi_DrawProgressBar(uint8_t progress)`，仅在 `ota_active` 为真时使用。

- [ ] **Step 1: 写出系统状态页的失败验收条件**

按 KEY1 切换当前程序页面。旧页面标题为 `OTA RESERVED`，并显示 `ADD OTA TASK LATER`，没有 OTA 进度条和当前固件版本。这是修改前应失败的验收结果。

- [ ] **Step 2: 确认失败结果**

在当前 AppSlotA 上按 KEY1。预期显示 `OTA RESERVED` 页面，而不是 `SYSTEM STATUS` 页面；页面中没有 `VERSION` 字段和方形进度条。

- [ ] **Step 3: 最小化实现状态页和进度条**

在 `app_ui.c` 顶部包含 OTA 配置头文件：

```c
#include "ota_port_config.h"
```

新增进度条函数，并限制进度最大为 100：

```c
static void AppUi_DrawProgressBar(uint8_t progress)
{
    uint16_t filled_width;

    if (progress > 100U)
    {
        progress = 100U;
    }

    LCD_DrawRectangle(40U, 350U, 280U, 374U, 2U, GREEN);
    filled_width = (uint16_t)((236U * progress) / 100U);
    AppUi_FillRect(42U, 352U, filled_width, 20U, GREEN);
    AppUi_FillRect((uint16_t)(42U + filled_width), 352U,
                   (uint16_t)(236U - filled_width), 20U, WHITE);
}
```

将 `AppUi_DrawOtaReservedPage()` 改为 `AppUi_DrawSystemStatusPage()`：标题为 `SYSTEM STATUS`，固定标签为 `ESP32`、`WIFI`、`MQTT`、`OTA`、`VERSION`，页尾为 `KEY1: ENV DASHBOARD`。将 `AppUi_DrawOtaStatus()` 改为每次先清除动态文本区域，再显示实际状态；当 `ota_active` 为真时绘制进度条和百分比，否则清除进度条区域。版本行使用：

```c
AppUi_WriteText(40U, 400U, 16U, "VERSION: " OTA_CURRENT_VERSION, BLACK, WHITE);
```

在 `AppUi_Render()` 的第二页分支中调用新的 `AppUi_DrawSystemStatusPage()`，其余缓存刷新流程保持不变。

- [ ] **Step 4: 确认系统状态页通过验收**

重新编译并下载 `AppSlotA`。按 KEY1 后应显示 `SYSTEM STATUS`，再次按 KEY1 应返回 `ENV DASHBOARD`。ESP32 未上报时应显示 `WAITING`；WiFi、MQTT 状态应与 ESP32 上报数据一致；OTA 下载中显示 `0%` 到 `100%` 进度条；非下载状态不残留旧进度条；版本显示为 AppSlotA 编译宏配置的版本号。

- [ ] **Step 5: 记录检查点**

确认只有 `Core/Src/app_ui.c` 修改，且 `app_ui.h`、`freertos.c`、`main.c` 均不需要修改。由于当前目录无 Git 仓库，不执行提交。

### Task 3: 完成回归检查与 CubeMX 兼容性确认

**Files:**
- Modify: 无。
- Test: Keil `AppSlotA` 完整编译、实机按键与状态刷新验收。

**Interfaces:**
- Consumes: 已完成的 `AppUi_Init()`、`AppUi_TogglePage()`、`AppUi_Render()`。
- Produces: 已验证的 AppSlotA 固件，不产生新的源代码接口。

- [ ] **Step 1: 执行完整编译检查**

在 Keil 选择 `AppSlotA` 后使用 `Rebuild all target files`。预期结果为 0 Error；如工程已有与本次无关的警告，记录其文件名和行号，不把它归因于 UI 修改。

- [ ] **Step 2: 执行实机回归检查**

按以下顺序检查：上电默认是环境仪表盘；无传感器时四个位置均为 `NO DATA`；按 KEY1 进入系统状态页；再次按 KEY1 返回仪表盘；ESP32 串口状态更新后两个页面都在 500 ms 内更新；OTA 期间传感器页仍可查看且状态页进度会变化。

- [ ] **Step 3: 确认 CubeMX 兼容性**

打开 `42_lcd_hal.ioc` 并确认不需要改变任何外设配置。重新生成代码前后检查 `Core/Src/app_ui.c` 仍存在，因为它是用户自建模块而不是 CubeMX 管理文件。

- [ ] **Step 4: 记录检查点**

保留 `Core/Src/app_ui.c` 和本计划文件。当前工程不是 Git 仓库，不执行提交。

## Plan Self-Review

- Spec coverage: 环境默认页、四项数据、ESP 状态摘要、KEY1 切页、WiFi/MQTT/OTA/版本信息、OTA 进度、无效数据、CubeMX 兼容性和验证流程均有对应任务。
- 完整性检查: 每项代码步骤均给出明确文件、函数、状态字段和验收结果。
- Type consistency: 所有状态读取均使用现有 `EnvData_t`、`Esp32Status_t` 和 `OTA_CURRENT_VERSION`；公开接口保持 `AppUi_Init`、`AppUi_TogglePage`、`AppUi_Render` 不变。
