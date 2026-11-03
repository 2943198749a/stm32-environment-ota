# Keil OTA 三工程恢复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建三个互不混编、可独立构建的 Keil 工程文件。

**Architecture:** 从已整理的 Target 配置生成三个单 Target `.uvprojx` 文件。单 Target 不会发生多 Target 的源文件继承或合并；三个工程继续引用同一份实际源代码。

**Tech Stack:** Keil MDK-ARM、STM32 HAL、FreeRTOS、STM32F103ZET6。

## Global Constraints

- 不保留恢复或检查脚本。
- 不删除 `Core`、`Drivers`、`Middlewares`、`portable_ota` 下的源文件。
- 保留现有 `42_lcd_hal.uvprojx` 与 `.uvoptx` 的带时间戳备份。
- Bootloader、A/B 的 IROM1 必须分别为 `0x08000000/0x7000`、`0x08008000/0x3C000`、`0x08044000/0x3C000`。

---

### Task 1: 生成三个单 Target Keil 工程

**Files:**
- Create: `MDK-ARM/Bootloader.uvprojx`
- Create: `MDK-ARM/AppSlotA.uvprojx`
- Create: `MDK-ARM/AppSlotB.uvprojx`
- Create: `MDK-ARM/42_lcd_hal.uvprojx.before_project_recovery.bak`
- Create: `MDK-ARM/42_lcd_hal.uvoptx.before_project_recovery.bak`

**Produces:** 每个 `.uvprojx` 仅包含一个 Target 和其专属文件清单。

- [ ] **Step 1: 备份当前多 Target 配置**

复制当前 `.uvprojx` 和 `.uvoptx` 为 `before_project_recovery.bak` 文件。

- [ ] **Step 2: 创建 Bootloader 工程**

从 Bootloader Target 生成 `Bootloader.uvprojx`。仅保留启动、最小 HAL Flash 与 OTA Bootloader 文件；地址为 `0x08000000`、大小为 `0x7000`。

- [ ] **Step 3: 创建 A/B 应用工程**

从 AppSlotA、AppSlotB Target 分别生成独立工程。两者均拥有完整应用、HAL 与 FreeRTOS 文件；地址和输出分别为 A 的 `0x08008000/Objects/AppSlotA/app_slot_a`、B 的 `0x08044000/Objects/AppSlotB/app_slot_b`。

### Task 2: 编译与镜像验证

**Files:**
- Modify (构建产物): `MDK-ARM/Objects/Bootloader/*`
- Modify (构建产物): `MDK-ARM/Objects/AppSlotA/*`
- Modify (构建产物): `MDK-ARM/Objects/AppSlotB/*`

**Produces:** 三个可用 AXF 文件。

- [ ] **Step 1: 编译 Bootloader**

Run:

```powershell
& 'C:\Keil_v5\UV4\UV4.exe' -r .\MDK-ARM\Bootloader.uvprojx -j0
```

Expected: `0 Error(s)`，并生成 `Objects/Bootloader/bootloader.axf`。

- [ ] **Step 2: 编译 AppSlotA 与 AppSlotB**

Run:

```powershell
& 'C:\Keil_v5\UV4\UV4.exe' -r .\MDK-ARM\AppSlotA.uvprojx -j0
& 'C:\Keil_v5\UV4\UV4.exe' -r .\MDK-ARM\AppSlotB.uvprojx -j0
```

Expected: 两者均为 `0 Error(s)`，并各自生成独立 AXF。

- [ ] **Step 3: 检查 A/B 大小**

读取 Keil map 文件中的 Program Size，确认 A/B 均小于 `0x3C000`。
