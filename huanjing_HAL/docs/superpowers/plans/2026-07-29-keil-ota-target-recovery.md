# Keil OTA 三 Target 恢复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 恢复 Bootloader、AppSlotA、AppSlotB 三个相互隔离、均可独立编译的 Keil Target。

**Architecture:** 保留现有源文件，只重建 `MDK-ARM/42_lcd_hal.uvprojx` 的 Target 文件清单。Bootloader 只链接最小启动和 OTA 校验代码；A/B 链接同一套完整 CubeMX + FreeRTOS 应用代码。每个 Target 使用独立 Flash 区域及输出目录。

**Tech Stack:** Keil MDK-ARM、STM32CubeMX HAL、FreeRTOS、STM32F103ZET6、PowerShell 静态配置检查。

## Global Constraints

- 不删除或覆盖 `Core/Src`、`Core/Inc`、`portable_ota` 下的任何用户源文件。
- 修改 `MDK-ARM/42_lcd_hal.uvprojx` 前，必须创建同目录备份。
- Bootloader 地址固定为 `0x08000000`，大小 `0x7000`；A/B 固定为 `0x08008000`/`0x08044000`，大小均为 `0x3C000`。
- Bootloader 不得链接 `main.c` 或 FreeRTOS；A/B 不得链接 `boot_main.c`。
- 所有输出目录和 AXF 文件名必须互不相同。

---

### Task 1: 建立失败态 Target 清单检查

**Files:**
- Create: `tests/test_keil_ota_targets.ps1`
- Test: `tests/test_keil_ota_targets.ps1`

**Consumes:** `MDK-ARM/42_lcd_hal.uvprojx`

**Produces:** 当 Target 数量、地址、应用入口、Bootloader 边界或输出目录错误时返回非零的 PowerShell 检查。

- [ ] **Step 1: 写入检查脚本**

脚本读取 `.uvprojx` XML，要求 Target 名称严格为 `Bootloader`、`AppSlotA`、`AppSlotB`；检查每个 Target 的文件路径、IROM1 及输出文件名。

- [ ] **Step 2: 运行检查，确认当前工程失败**

Run:

```powershell
& .\tests\test_keil_ota_targets.ps1
```

Expected: FAIL，原因包含旧 `42_lcd_hal` Target 或 A/B 缺少 `main.c`。

### Task 2: 重建 Target 文件归属和输出设置

**Files:**
- Modify: `MDK-ARM/42_lcd_hal.uvprojx`
- Create: `MDK-ARM/42_lcd_hal.uvprojx.before_target_recovery.bak`

**Consumes:** 源文件目录 `Core/Src`、HAL Driver、FreeRTOS Source、`portable_ota/stm32/ota_core`。

**Produces:** 三个 Target 的独立 Keil XML 配置。

- [ ] **Step 1: 备份工程文件**

复制 `.uvprojx` 到 `.uvprojx.before_target_recovery.bak`，不修改源代码目录。

- [ ] **Step 2: 配置 Bootloader Target**

保留启动文件、`system_stm32f1xx.c`、`stm32f1xx_hal.c`、`stm32f1xx_hal_cortex.c`、`stm32f1xx_hal_flash.c`、`stm32f1xx_hal_flash_ex.c`、`boot_main.c`、`ota_storage.c`、`ota_metadata.c`、`ota_integrity.c`、`sha256.c`。设置 OTA Include Paths、IROM1 `0x08000000/0x7000` 和输出 `Objects/Bootloader/bootloader`。

- [ ] **Step 3: 配置 AppSlotA Target**

加入所有 `Core/Src/*.c`（`system_stm32f1xx.c` 只保留一次）、所有已启用 HAL 驱动、FreeRTOS 内核、CMSIS RTOS2、heap_4 和 ARM_CM3 port；不加入任何 `portable_ota` 源文件。设置 IROM1 `0x08008000/0x3C000` 与输出 `Objects/AppSlotA/app_slot_a`。

- [ ] **Step 4: 配置 AppSlotB Target**

复制 A 的完整应用文件列表，仅把 IROM1 改为 `0x08044000/0x3C000`，输出改为 `Objects/AppSlotB/app_slot_b`。

- [ ] **Step 5: 删除旧默认 Target**

移除 `42_lcd_hal` Target，避免用户误把应用下载到 Bootloader 区。

### Task 3: 验证配置并编译

**Files:**
- Test: `tests/test_keil_ota_targets.ps1`
- Modify (仅构建产物): `MDK-ARM/Objects/*`

**Consumes:** 修复后的 `.uvprojx`。

**Produces:** 三个可编译 AXF 文件。

- [ ] **Step 1: 运行 Target 静态检查**

Run:

```powershell
& .\tests\test_keil_ota_targets.ps1
```

Expected: PASS。

- [ ] **Step 2: 分别编译三个 Target**

Run:

```powershell
& 'C:\Keil_v5\UV4\UV4.exe' -r .\MDK-ARM\42_lcd_hal.uvprojx -t Bootloader -j0
& 'C:\Keil_v5\UV4\UV4.exe' -r .\MDK-ARM\42_lcd_hal.uvprojx -t AppSlotA -j0
& 'C:\Keil_v5\UV4\UV4.exe' -r .\MDK-ARM\42_lcd_hal.uvprojx -t AppSlotB -j0
```

Expected: 每次均显示 `0 Error(s)`。

- [ ] **Step 3: 检查 A/B 镜像大小**

读取 Keil map 或 AXF 生成信息，确认 AppSlotA 与 AppSlotB 均小于 `0x3C000`。
