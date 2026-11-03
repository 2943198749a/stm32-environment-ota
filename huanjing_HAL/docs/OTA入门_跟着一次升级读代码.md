# OTA 入门：跟着一次升级读代码

本说明对应当前工程。先看流程，再查函数，不必从 SHA-256 算法开始读。

## 当前版本的最短阅读路线

代码现已按主流程优先排列，函数内部行为保持不变：

1. `ota_app.c`：跳过辅助函数声明，从 Init → Begin → Write → Finalize → Abort 往下读，然后读启动确认。参数校验和擦除辅助函数放在末尾。
2. `ota_uart.c`：从 Init → Process → HandleControlLine → HandleBegin → AcknowledgeFrame 往下读。接着是逐字节接收细节，应答和 CRC 算法放在后面。
3. `boot_main.c`：SelectSlot 选择 A/B，再由 JumpToSlot 进入程序。Bootloader 调试输出保留。
4. `freertos.c`：只看 StartOtaTask 和 RtosOta_ConfirmWhenStable，连接新程序启动确认过程。

### 华为云上传与固件下载的正确入口

完整 ESP32-C3 网关是 `portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway/esp32c3_ota_gateway.ino`，包括环境数据上报和 OTA 固件下载。
`huanjing_HAL/arduino/esp32c3_huawei_cloud/esp32c3_huawei_cloud.ino` 是普通上报示例，不能当作完整 OTA 网关使用。

```text
环境上传：STM32 采集 → ENV 串口消息 → ESP32-C3 → MQTT → 华为云
固件下载：华为云 OTA 通知 → ESP32-C3 下载 → 串口分块 → STM32 写入非运行区
升级生效：整包检查 → 保存待启动记录 → 复位 → Bootloader 选区 → 新应用确认
```

本轮简化只调整 STM32 OTA 函数排列和阅读注释，保留完整网关、串口协议、A/B 分区、校验、启动确认和回退规则。



## 一、先认清几个名字

| 名字 | 含义 | A 正在运行、准备更新 B 时 |
|---|---|---|
| running_slot | CPU 当前运行的程序区 | A |
| target_slot | 本次下载要写入的区 | B |
| active_slot | 启动记录中已确认的程序区 | 确认新程序前仍是 A |
| pending_slot | 启动记录中等待试运行、确认的区 | 下载检查通过后是 B |
| written_size | 本次已经写入的字节数 | 每写好一块就增加 |
| expected_size | 开始升级时声明的总字节数 | 收完后必须与 written_size 相等 |
| offset | 本块距离目标区起点的字节数 | 必须接在上一块后面 |

A/B 是两份完整应用的存放位置，不是两个同时运行的任务。
Bootloader 是复位后先执行的小程序，负责选择并跳转到一个应用区。

## 二、顺着一次 A → B 升级看

```text
ESP32 获取固件，通过串口发送
  ↓ OTA_BEGIN：版本、目标区 B、长度、摘要
OtaUart_HandleBegin
  ↓
OtaApp_Begin：检查参数 → 擦除 B → 记住本次下载信息
  ↓ 多次接收二进制分块
OtaUart_AcknowledgeFrame：检查序号 → OtaApp_Write → 回复 ACK
  ↓ OTA_END
OtaApp_Finalize：检查收齐 → 检查摘要和入口 → 保存待启动记录
  ↓ 回复 OTA_OK，然后复位
Bootloader_SelectSlot：检查 B → 保存试启动次数 → 选择 B
  ↓
Bootloader_JumpToSlot：进入 B 的启动入口
  ↓ B 中的 OTA 任务开始计时
RtosOta_ConfirmWhenStable：满 10 秒且需要确认 → OtaApp_ConfirmRunning
  ↓
active_slot = B，pending_slot = NONE，然后发送 BOOT_OK
```

下一次 B → A 升级时，两个区的角色交换。

## 三、第一遍只看这四个函数

打开 `user/OTA/Src/ota_app.c`，按下面顺序读：

1. `OtaApp_Begin`：准备好再动 Flash。参数错误时不能擦除程序。
2. `OtaApp_Write`：检查本块 → 写入 → 读回比较 → 更新摘要和进度。
3. `OtaApp_Finalize`：下载结束后检查完整性，再保存“下次试启动”的记录。
4. `OtaApp_ConfirmRunning`：新程序启动后，把当前区改成已确认区。

`Finalize` 和 `ConfirmRunning` 不是同一件事：前者说明下载并检查完成，后者发生在新程序启动之后。
`Finalize` 本身不复位；串口处理先回复 `OTA_OK`，再调用复位接口。

例如每块 512 字节：第一块 offset=0，第二块 offset=512，第三块 offset=1024。
不允许跳着写，否则程序内容和摘要顺序可能不一致。
最后一块可以比 512 字节小，但仍需满足偶数字节、总长度等限制。

## 四、第二遍再看串口接收

打开 `user/OTA/Src/ota_uart.c`，先找 `OtaUart_Process`。
它从串口缓存逐字节取数据，交给两种处理：

- 文本命令：开始 `OTA_BEGIN`、结束 `OTA_END`、取消 `OTA_ABORT`。
- 二进制数据帧：帧头、序号、长度、程序数据、CRC16。

`switch` 中的状态可以理解为“现在等着收哪一部分”。
串口一次给一个字节，所以必须记住已经收到哪里，不能假定一次就收到完整数据包。

`ACK` 表示这块已接受，`NACK` 表示没有接受，并携带当前期待的序号。
USART2 中断只存字节；真正的解析和 Flash 写入在 OTA 任务中进行。

## 五、三种检查分别做什么

| 检查 | 用途 |
|---|---|
| CRC16 | 检查串口收到的一块数据 |
| SHA-256 | 下载结束时，把全部接收数据的摘要与期望摘要比较 |
| CRC32 | 下载完成后计算 Flash 中程序的校验值，保存在启动记录中，Bootloader 复查时使用 |

这些是完整性检查。初学时先知道输入、结果和失败后停止升级即可，算法内部可以最后再看。

## 六、启动记录为什么要保存

普通变量在复位后会丢失，所以必须把“已确认区、待确认区、试启动次数”等写进 Flash。
`ota_metadata.c` 负责组织、检查记录，`ota_storage.c` 负责读写 Flash 中的记录副本。

新程序未确认时，旧的 active 区仍保留。后续复位进入 Bootloader 时，它会检查待启动程序和尝试次数，决定继续试启动还是回到旧区。
注意：这不等于新程序一旦卡死就会立即自动回滚；需要先发生复位，Bootloader 才有机会重新选择。
当前的 10 秒确认只检查时间和待确认记录，不是对所有传感器、LCD、网络的完整自检。

## 七、最后再看底层

- `ota_layout.h`：Flash 分区地址和容量，必须与编译链接地址配套。
- `ota_port.c`：具体串口收发与复位接口。
- `boot_main.c`：先看 `Bootloader_SelectSlot`；跳转中的寄存器和栈操作可以后读。
- `sha256.c`：完整性算法实现，理解升级主流程不要求先读懂它。

主工程 OTA 维护源在 `huanjing_HAL/user/OTA`。
`portable_ota/stm32/ota_core` 是同步副本；Bootloader 的代码实际位于
`portable_ota/stm32/stm32f103_example/bootloader/boot_main.c`，因此不能直接删除 portable_ota。

这次整理保留了现有函数接口、协议消息、分区布局和确认规则。

## 八、内部状态已改成普通变量

`ota_app.c` 中原来的 `OtaAppState_t`、`g_ota_app` 已移除。
现在直接使用以下文件内变量，不需要先找结构体成员：

| 变量 | 含义 |
|---|---|
| download_busy | 是否正在下载 |
| download_running_slot | 本次下载期间运行的区，不能擦写 |
| download_target_slot | 新程序要写入的区 |
| download_total_bytes | 整份新程序有多少字节 |
| download_written_bytes | 已经写好并读回检查的字节数 |
| download_expected_sha256 | 期望的整包摘要文本 |
| download_sha256 | 摘要算法的中间结果，仍使用算法规定的类型 |

例如 `download_written_bytes += length;` 就是“已写入字节数加上本块长度”。
这些变量前面的 `static` 表示只能在当前 `.c` 文件中直接访问。

`ota_uart.c` 中的 `OtaUartState_t`、`g_ota_uart` 也已移除。
变量按四组排列：整个接收过程、当前数据块、文本命令、处理结果。
`uart_next_sequence` 是下一块的序号；`uart_received_sequence` 是当前收到的序号。
只有序号正确且写入成功后，才增加 `uart_written_bytes` 和 `uart_next_sequence`。

两个文件中的字节计数各有用途：`download_written_bytes` 由写入模块维护，
用于连续写入和整包长度检查；`uart_written_bytes` 由串口模块维护，
用于告诉写入模块本块应该放在哪里。它们的分工与拆开前相同。

初始化、开始新下载以及取消下载时，原来整块清零的位置已改为逐项清零，
包括摘要、文本缓冲和数据缓冲。`OtaBootRecord_t` 仍保留，因为它定义了
要写进 Flash、供下次开机读取的完整记录。`Sha256Context_t` 和 HAL 结构体也保留。

## 九、主工程进一步精简

- STM32 主工程已移除 `ota_cloud.c/.h` 和 Keil A/B 编译项。云端 JSON 由 ESP32-C3 处理；STM32 接收串口命令和固件数据。这两个文件仅作为通用可选模块保留在 `portable_ota/stm32/ota_core`，不再由主工程同步。
- 未被调用的 `OtaApp_ResetToBootloader` 已移除。实际升级仍由 `ota_uart.c` 调用 `OtaPort_ResetToBootloader` 复位。
- 构造、验证测试帧的两个 `ForTest` 函数已移入 `tests/test_ota_uart_compile.c`，并改为测试文件内部函数。正式 OTA 头文件不再声明它们。
- 下载状态统一由 `OtaApp_Init()` 清零。`Begin` 在参数检查和擦除成功后调用它，再填写新下载的信息；`Abort` 调用它取消本次下载。清零只影响 RAM，不擦除 Flash 或启动记录。
- Bootloader 的串口调试输出保持原样。

测试源文件仍使用正式代码中的 `OtaUart_Crc16`，检查已知 CRC 向量、帧构造和损坏帧识别。编译电脑端测试时，应定义 `OTA_UART_HOST_TEST`，同时编译 `tests/test_ota_uart_compile.c` 与 `user/OTA/Src/ota_uart.c`。
