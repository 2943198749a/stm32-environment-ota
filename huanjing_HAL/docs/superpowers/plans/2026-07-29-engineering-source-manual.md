# 工程源码说明手册 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 基于 `huanjing_HAL` 当前真实代码生成一份中文 Word 工程源码说明手册，使初学者能知道代码写在哪里、每个模块做什么、如何避免 CubeMX 覆盖自写代码。

**Architecture:** 手册以工程目录与运行流程为主线，再分别说明 CubeMX 生成层、自写应用层、FreeRTOS 任务层和 OTA 三 Target 层。所有函数与路径均从当前源码提取；不修改用户的嵌入式工程代码。

**Tech Stack:** STM32CubeMX、HAL、FreeRTOS、Keil MDK、python-docx、LibreOffice 渲染检查。

## Global Constraints

- 仅创建说明文档和内部计划，不改动 `Core`、`user`、`MDK-ARM` 下现有源码。
- 使用中文，面向初学者，术语首次出现时说明其作用。
- 所有“代码位置”写为工程内相对路径，并明确 CubeMX USER CODE 区域。
- 交付前必须渲染 DOCX，并检查所有页面无截断、重叠、乱码。

---

### Task 1: 提取工程实际结构与接口

**Files:**
- Read: `Core/Src/main.c`
- Read: `Core/Src/freertos.c`
- Read: `Core/Src/*.c`, `Core/Inc/*.h`
- Read: `user/OTA/Src/*.c`, `user/OTA/Inc/*.h`
- Read: `MDK-ARM/*.uvprojx`

- [ ] **Step 1: 建立源码清单**

读取每个自写模块的头文件和源文件，记录公开函数、主要调用者及文件职责。

- [ ] **Step 2: 建立流程关系**

从 `main.c`、`freertos.c` 与 OTA 入口提取初始化顺序、任务职责和升级调用链。

- [ ] **Step 3: 核对 CubeMX 边界**

确定生成文件与自写文件的边界，记录可安全编写的 `USER CODE BEGIN/END` 位置。

### Task 2: 编写 Word 技术手册

**Files:**
- Create: `output/STM32F103ZET6_工程源码说明手册.docx`

- [ ] **Step 1: 编写目录与阅读路线**

用目录树、阅读顺序与文件归属表，说明从哪里开始理解工程。

- [ ] **Step 2: 编写运行核心说明**

解释 `main.c` 初始化、FreeRTOS 任务、传感器采集、LCD、ESP32 串口、OTA 暂停 ENV 上报的关系。

- [ ] **Step 3: 编写模块逐文件说明**

按 `.c/.h` 成对列出路径、职责、关键函数、调用方式、是否由 CubeMX 生成及注意事项。

- [ ] **Step 4: 编写 OTA 三 Target 说明**

说明 Bootloader、AppSlotA、AppSlotB 的职责、Flash 地址、Keil Target 对应关系及升级流程。

### Task 3: 校验并交付

**Files:**
- Verify: `output/STM32F103ZET6_工程源码说明手册.docx`

- [ ] **Step 1: 结构校验**

检查 Word 中文件路径、函数名称、章节标题和链接是否与当前源码一致。

- [ ] **Step 2: 渲染视觉检查**

使用 `render_docx.py` 生成页面 PNG，逐页检查中文字符、表格、代码块与分页。

- [ ] **Step 3: 修正后交付**

修复发现的格式问题，重新渲染确认，并仅交付最终 DOCX。
