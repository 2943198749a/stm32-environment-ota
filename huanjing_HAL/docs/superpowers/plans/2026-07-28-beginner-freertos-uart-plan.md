# 初学者版 FreeRTOS 与 ESP32 UART 改造计划

> **执行说明：** 按本文步骤逐项修改、先运行对应检查，再进行下一项。项目当前不是 Git 仓库，因此不创建 worktree，也不包含提交步骤。

**目标：** 保留传感器、LCD 和 ESP32 UART 三个 FreeRTOS 任务；移除 ESP-AT、MQTT 和云平台代码；将任务与模块改成初学者容易理解、并带详细中文注释的写法。

**结构：** 传感器任务每秒采集一次数据，使用极短的临界区复制最新数据；LCD 任务每 500 ms 读取并显示数据；UART 任务每 10 ms 非阻塞接收 ESP32 发来的字符。ESP32 由 Arduino IDE 独立烧录，STM32 仅负责 USART2 字符串收发。

**工具：** STM32 HAL、CMSIS-RTOS v2（FreeRTOS）、STM32CubeMX、Keil MDK、PowerShell 静态检查。

---

## 任务 1：先更新检查脚本，锁定精简后的接口

**涉及文件：**

- 新建：`tests/test_beginner_rtos_uart.ps1`
- 新建：`tests/test_esp32_uart_module.ps1`
- 修改：`tests/test_lcd_ota_ui.ps1`
- 修改：`tests/test_freertos_task_creation_check.ps1`
- 删除：`tests/test_esp32_at_receive.ps1`
- 删除：`tests/test_arduino_esp32_mode.ps1`
- 删除：`tests/test_rtos_mutex_stage.ps1`
- 删除：`tests/test_lcd_startup_stages.ps1`
- 删除：`tests/test_lcd_boot_splash.ps1`

**步骤：**

1. 编写检查，要求 `freertos.c` 只保留传感器、界面、UART 三个任务。
2. 编写检查，要求新增 UART 模块使用 `huart2`，接收时超时为 `0`，以保证 UART 任务不会阻塞其他任务。
3. 更新 OTA 页面检查：页面只保留“后续 OTA 预留”的静态提示，不再依赖 ESP-AT 网络状态。
4. 删除已不符合目标的 ESP-AT 与启动诊断检查。

**验证命令：**

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\test_beginner_rtos_uart.ps1
powershell -ExecutionPolicy Bypass -File .\tests\test_esp32_uart_module.ps1
```

预期：此时检查会失败，因为实现尚未完成。

---

## 任务 2：新增独立、简单的 ESP32 UART 模块

**涉及文件：**

- 新建：`Core/Inc/esp32_uart.h`
- 新建：`Core/Src/esp32_uart.c`
- 修改：`MDK-ARM/42_lcd_hal.uvprojx`

**步骤：**

1. 在头文件声明 `Esp32Uart_Init`、`Esp32Uart_Process`、`Esp32Uart_SendString` 和 `Esp32Uart_ReadLine`，并用中文说明每个函数用途。
2. 在源文件中只使用 CubeMX 已生成的 `huart2`（PA2/PA3）。`Init` 只清空接收状态；不重新初始化 UART，避免和 CubeMX 生成代码冲突。
3. `Process` 每次最多读取一个字符，调用 `HAL_UART_Receive(..., 0)`；收到换行符后将一行标记为可读取。这样任务不会等待串口数据。
4. `SendString` 调用 `HAL_UART_Transmit` 发送文本；`ReadLine` 安全复制已收到的一整行文本。
5. 把新 `.c` 和 `.h` 文件加入 Keil 工程；不在 CubeMX 生成文件中直接添加业务逻辑。

**验证命令：**

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\test_esp32_uart_module.ps1
```

---

## 任务 3：重写 FreeRTOS 三任务逻辑

**涉及文件：**

- 修改：`Core/Src/freertos.c`
- 修改：`42_lcd_hal.ioc`
- 可能修改：`Core/Inc/FreeRTOSConfig.h`

**步骤：**

1. 清除 ESP-AT 初始化、网络状态、MQTT 处理、互斥锁启动诊断以及调试启动页面调用。
2. 保留 `latestEnvData`，用两个很短的中文注释辅助函数保存/复制数据；复制结构体时使用 `taskENTER_CRITICAL` 与 `taskEXIT_CRITICAL`，避免 LCD 正好读到一半的数据。
3. 在 `StartDefaultTask` 内部先调用 `EnvSensor_Init`，再循环读取传感器。不得在 `MX_FREERTOS_Init` 中初始化传感器，因为未接 I2C 设备时可能在调度器启动前卡住。
4. 在 `StartTask02` 初始化按键和 UI，每 500 ms 显示一次最新传感器数据；按键切换环境页与 OTA 预留页。
5. 在 `StartTask03` 初始化 UART 模块，并每 10 ms 调用 `Esp32Uart_Process`。该任务不包含 AT 指令，也不调用云平台。
6. 将 `.ioc` 中第三个任务名称改为 `uarttask`，保持函数入口 `StartTask03`，使下次 CubeMX 生成仍能保留任务结构。
7. 保留并检查 FreeRTOS 堆大小足以创建三个动态任务；如必须改动 `FreeRTOSConfig.h`，只改该配置值并给出原因。

**验证命令：**

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\test_beginner_rtos_uart.ps1
powershell -ExecutionPolicy Bypass -File .\tests\test_freertos_task_creation_check.ps1
powershell -ExecutionPolicy Bypass -File .\tests\test_freertos_heap_capacity.ps1
```

---

## 任务 4：精简 LCD 界面并保留 OTA 预留入口

**涉及文件：**

- 修改：`Core/Inc/app_ui.h`
- 修改：`Core/Src/app_ui.c`
- 修改：`Core/Src/main.c`

**步骤：**

1. 删除 `Esp32C3Status_t`、网络状态绘制、OTA 进度和启动阶段接口，UI 只依赖 `EnvData_t`。
2. 保留环境数据页的温湿度、光照、空气质量显示；布局绘制与数值刷新分开，降低 500 ms 刷新时的闪烁。
3. 保留第二页，显示“OTA 功能预留”和后续添加 OTA 任务的位置说明；该页不执行升级、不依赖 ESP32 AT 固件。
4. `main.c` 的 USER CODE 区仅保留 LCD 初始化和 ESP32 使能脚初始化。删除启动自检/调试页面调用。
5. 所有业务修改都位于 `/* USER CODE BEGIN ... */` 区域或独立 `.c/.h` 文件；不修改 CubeMX 所有权的初始化函数主体。

**验证命令：**

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\test_lcd_ota_ui.ps1
powershell -ExecutionPolicy Bypass -File .\tests\test_sensor_init_in_task.ps1
```

---

## 任务 5：移除 ESP-AT 模块与工程引用

**涉及文件：**

- 删除：`Core/Inc/esp32_c3.h`
- 删除：`Core/Src/esp32_c3.c`
- 修改：`MDK-ARM/42_lcd_hal.uvprojx`

**步骤：**

1. 从 Keil 工程中移除 `esp32_c3.c` 和 `esp32_c3.h` 条目。
2. 删除 ESP-AT 模块的源文件和头文件。
3. 在 `Core`、Keil 工程和测试目录中搜索 `Esp32C3`、`esp32_c3`、`MQTT`、`ESP-AT`，确认没有业务代码引用旧模块。

**验证命令：**

```powershell
rg -n "Esp32C3|esp32_c3|MQTT|ESP-AT" Core MDK-ARM tests
```

预期：命令没有匹配结果，或只显示说明旧模块已移除的历史文档（不在编译目录中）。

---

## 任务 6：完整验证和 CubeMX 回归检查

**涉及文件：**

- 所有以上文件

**步骤：**

1. 运行全部保留的 PowerShell 检查。
2. 用 Keil 重建 `MDK-ARM/42_lcd_hal.uvprojx`，检查 `0 Error(s), 0 Warning(s)`。
3. 检查 `.ioc` 的 USART2、FreeRTOS 三任务及 `KeepUserCode=true` 配置仍存在。
4. 人工复核：传感器初始化位于传感器任务、LCD 不显示旧的 `RTOS INIT/MUTEX OK` 调试文字、UART 任务不阻塞。

**验证命令：**

```powershell
Get-ChildItem .\tests\*.ps1 | ForEach-Object { & powershell -ExecutionPolicy Bypass -File $_.FullName }
Set-Location .\MDK-ARM
& 'C:\Keil_v5\UV4\UV4.exe' -r 42_lcd_hal.uvprojx -j0
```

预期：全部检查通过，Keil 输出 `0 Error(s), 0 Warning(s)`。

---

## 验收标准

1. STM32 工程仅保留三个 FreeRTOS 任务：传感器、LCD、ESP32 UART。
2. ESP32 UART 通信只使用 USART2，不包含 AT/MQTT/云平台指令。
3. LCD 有环境数据页和 OTA 功能预留页。
4. 未接传感器时，FreeRTOS 调度器仍能启动；传感器初始化只在传感器任务中执行。
5. 所有新增或重写的业务代码包含面向初学者的详细中文注释。
6. 用户重新从 CubeMX 生成代码后，业务模块及 USER CODE 区的调用不会被覆盖。
