# 整个程序怎样运行：正常运行与华为云 OTA 函数调用图

依据：2026-09-17 当前工作区源码。以下是静态调用关系和控制流程解释，不代表已完成联网或硬件实测。示例以当前运行 Slot A、升级到 Slot B 为主；反向升级的过程相同。

## 阅读图的方法

- 节点第一行讲“在干什么”，下一行给真实函数名。
- 实线箭头表示图中标注的调用或先后处理顺序；不能把所有先后关系理解成前一个函数直接调用后一个函数。
- 虚线箭头表示串口/MQTT 消息、共享数据或任务调度；两端不是普通 C 函数调用。
- Bootloader 的 main 与应用 main 属于两个独立程序。ESP32 的 setup/loop 又运行在另一颗芯片上。
- FreeRTOS 四个任务由调度器轮流执行；传感器任务不会直接调用 UI 任务。

## 图 1：整个工程的职责地图

```mermaid
flowchart TB
  SYS["环境监测与远程升级系统"]
  SYS --> B["STM32 开机选程序<br/>boot_main.c 的 main()"]
  SYS --> A["STM32 应用初始化与任务调度<br/>main() / MX_FREERTOS_Init()"]
  SYS --> E["ESP32 网络与云端网关<br/>setup() / loop()"]
  B --> BS["选稳定程序或试运行新程序<br/>Bootloader_SelectSlot()"]
  B --> BJ["交出 CPU 控制权<br/>Bootloader_JumpToSlot()"]
  A --> T1["采集环境<br/>StartDefaultTask()"]
  A --> T2["按键和屏幕<br/>StartTask02()"]
  A --> T3["上传环境数据<br/>StartTask03()"]
  A --> T4["接收升级数据与启动确认<br/>StartOtaTask()"]
  E --> NET["维护 Wi-Fi / MQTT<br/>HuaweiIotda::update()"]
  E --> RX["处理 STM32 的 ENV / BOOT_OK<br/>processStm32Lines()"]
  E --> OTA["执行等待中的升级<br/>processPendingOta()"]
  E --> STATUS["定期返回网络状态<br/>sendPeriodicStatus()"]
```

## 图 2：STM32 从上电到四个任务运行

以下为主要执行顺序。启动文件的 Reset_Handler 完成启动流程后进入各自 main。

```mermaid
flowchart TB
  P["芯片上电或系统复位"] --> B["进入 Bootloader<br/>boot_main.c: main()"]
  B --> BI["准备调试串口<br/>Bootloader_DebugInit()"]
  BI --> SEL["读取记录并选择槽位<br/>Bootloader_SelectSlot()"]
  SEL --> LOG["打印 A / B / 无应用<br/>Bootloader_DebugText()"]
  LOG --> J["检查向量表、切换栈和中断表<br/>Bootloader_JumpToSlot()"]
  J --> R["进入目标应用启动入口<br/>Reset_Handler"]
  R --> M["进入应用程序<br/>Core/Src/main.c: main()"]
  M --> H["初始化 HAL 和时钟<br/>HAL_Init() / SystemClock_Config()"]
  H --> IO["初始化 GPIO、FSMC、串口、I2C、ADC<br/>MX_GPIO_Init() / MX_FSMC_Init()<br/>MX_USART1_UART_Init() / MX_USART2_UART_Init()<br/>MX_I2C1_Init() / MX_ADC1_Init()"]
  IO --> LCD["初始化屏幕并释放 ESP32 使能<br/>LCD_Init() / Board_Esp32Enable_Init()"]
  LCD --> K["初始化内核<br/>osKernelInitialize()"]
  K --> F["初始化共享模块并创建任务<br/>MX_FREERTOS_Init()"]
  F --> ST["开始调度<br/>osKernelStart()"]
  ST -. 调度 .-> T1["传感器任务<br/>StartDefaultTask()"]
  ST -. 调度 .-> T2["显示任务<br/>StartTask02()"]
  ST -. 调度 .-> T3["发送任务<br/>StartTask03()"]
  ST -. 调度 .-> T4["升级任务<br/>StartOtaTask()"]
```

`MX_FREERTOS_Init()` 先清空 latestEnvData，再调用 `Esp32Status_Init()`、`Esp32Uart_Init()`、`OtaApp_Init()`、`OtaUart_Init(OTA_APPLICATION_SLOT)`，随后用 `osThreadNew()` 创建四个任务。`RtosTasks_AreCreated()` 检查句柄；失败则进入 `Error_Handler()`。

`Esp32Uart_Init()` 调用 `HAL_UART_Receive_IT()` 开始接收字节，所以真正开始收串口并不需要等到升级指令到来。

`LCD_Init()` 内依次调用 `MX_FSMC_Init()`、`LCD_Reset()`、`LCD_BGOn()`、`LCD_RegConfig()`。应用 main 中的 while(1) 不是日常业务循环；内核成功启动后，业务在任务中执行。

## 图 3：正常采样、显示和上传是怎么连起来的

```mermaid
flowchart TB
  T1["传感器任务：先初始化，再循环采集<br/>StartDefaultTask()"]
  T1 --> I["初始化传感器<br/>EnvSensor_Init()<br/>Dht11_Init() / Bh1750_Init()"]
  T1 --> U["每轮更新环境数据<br/>EnvSensor_Update()"]
  U --> D["读取温湿度<br/>Dht11_Read()"]
  U --> L["读取光照<br/>Bh1750_ReadLux()"]
  U --> Q["读取气体传感器 ADC<br/>Mq135_Read()"]
  U -->|返回后任务继续| SAVE["复制模块数据到共享快照<br/>RtosData_SaveEnvironment()<br/>EnvSensor_GetData()"]
  SAVE -. 数据 .-> DATA["latestEnvData<br/>环境值及有效标志"]
  SAVE --> WAIT["让出 CPU 后进入下一轮<br/>osDelay(1000)"]
  WAIT --> U
  DATA -. 复制 .-> COPYUI["显示任务读取快照<br/>StartTask02()<br/>RtosData_CopyEnvironment()"]
  COPYUI --> RENDER["结合网络状态绘制页面<br/>Esp32Status_Get() / AppUi_Render()"]
  KEY["检测 KEY1 按下并切页<br/>AppKey_PageTogglePressed()<br/>AppUi_TogglePage()"] --> RENDER
  RENDER --> DRAW["绘制环境页或系统状态页<br/>AppUi_DrawEnvironmentValues()<br/>AppUi_DrawSystemStatus()"]
  DATA -. 复制 .-> TX["发送任务读取快照<br/>StartTask03()<br/>RtosData_CopyEnvironment()"]
  TX --> CHECK{"数据全有效且未接收升级？<br/>RtosData_IsEnvironmentReady()<br/>!OtaUart_IsBusy()"}
  CHECK -->|是| FORMAT["组装 ENV 文本并发送<br/>RtosData_SendEnvironment()<br/>Esp32Uart_SendString()<br/>HAL_UART_Transmit()"]
  CHECK -->|否| SKIP["本轮不上传<br/>osDelay(1000)"]
  FORMAT -. USART2 到 ESP32 .-> ER["读出完整文本行<br/>processStm32Lines()<br/>Stm32OtaLink::readLine()"]
  ER --> PUB["将 ENV 转为云端属性 JSON<br/>HuaweiIotda::publishEnvironment()"]
  PUB --> SEND["发送 MQTT 消息<br/>HuaweiIotda::publishJson()<br/>mqttClient_.publish()"]
  SEND -. properties/report .-> CLOUD["华为云环境属性"]
```

循环末尾的延时分别为传感器任务 1000 tick、UI 任务 500 tick、发送任务 1000 tick、OTA 任务 1 tick。当前 tick 为 1 ms；实际每轮时间还包含采集、绘图、等待和调度耗时，不是严格的定时采样周期。

UI 任务初次运行会调用 `AppKey_Init()`、`AppUi_Init()`。它根据快照变化局部刷新，而非每次都清屏。传感器数据通过临界区内的结构体复制共享，绘图和发送在临界区外执行。MQ135 当前显示的是未标定估算值。

## 图 4：华为云下发指令，究竟谁先接收

ESP32 的 `setup()` 调用 `stm32Link.begin()`、`huawei.begin()`；后者通过 `setCallback(HuaweiIotda::mqttCallback)` 注册回调。

```mermaid
flowchart TB
  LOOP["ESP32 主循环<br/>loop()"] --> UP["维护联网状态<br/>HuaweiIotda::update()"]
  UP --> WIFI["连接 Wi-Fi<br/>ensureWifi()"]
  UP --> MQTT["连接 MQTT、订阅升级事件主题<br/>ensureMqtt()"]
  UP --> ML["处理 MQTT 心跳与入站消息<br/>mqttClient_.loop()"]
  CLOUD["华为云下发升级包描述<br/>版本、URL、大小、SHA256、目标槽"] -. sys/events/down .-> ML
  ML -->|库触发已注册回调| CB["云端消息的项目入口<br/>HuaweiIotda::mqttCallback()"]
  CB --> HM["解析 JSON、识别升级事件<br/>HuaweiIotda::handleMqttMessage()"]
  HM --> STORE["保存待处理包描述<br/>pendingPackage_<br/>packagePending_ = true"]
  STORE -. 回调返回后由主循环取走 .-> PO["执行待处理升级<br/>processPendingOta()"]
  PO --> TAKE["取出包描述并清除待处理标记<br/>HuaweiIotda::takePendingPackage()"]
  TAKE -->|有包且未在传输| DL["下载并传给 STM32<br/>downloadAndTransfer()"]
  DL --> HTTP["建立下载连接、GET、检查长度<br/>http.begin() / http.GET() / http.getSize()"]
  HTTP -->|下载响应可用| BEGIN["通知 STM32 准备接收<br/>Stm32OtaLink::beginTransfer()"]
```

这里云端先发的是“升级通知及下载信息”，并不是把整份 bin 直接塞进 MQTT。固件字节随后由 `downloadAndTransfer()` 获取的 HTTP 流提供。

`handleMqttMessage()` 接受 `$ota` 下的 `firmware_upgrade` / `firmware_upgrade_v2`，提取 version、url、file_size、sign、custom_info 中的 slot。`version_query` 会调用 `reportCurrentVersion()`，不会进入升级。

回调负责登记任务，不直接下载。ESP32 `loop()` 的实际顺序为 `huawei.update()` → `processStm32Lines()` → `processPendingOta()` → `sendPeriodicStatus()`；前一个函数返回后才执行下一个。

## 图 5：STM32 收串口的共同入口

无论 STATUS、OTA_BEGIN、二进制帧还是 OTA_END，都先走这一条通道。中断只缓存；OTA 任务负责解析和写 Flash。

```mermaid
flowchart TB
  W["ESP32 发出串口字节"] -. UART 硬件 .-> IRQ["串口中断入口<br/>USART2_IRQHandler()"]
  IRQ --> HAL["HAL 处理接收中断<br/>HAL_UART_IRQHandler()"]
  HAL -->|接收完成回调| CB["存入环形缓冲区<br/>HAL_UART_RxCpltCallback()"]
  CB --> REARM["重新开启下一字节接收<br/>HAL_UART_Receive_IT()"]
  CB -. 保存字节 .-> BUF["g_rx_buffer<br/>环形缓冲区"]
  TASK["OTA 任务循环<br/>StartOtaTask()"] --> PROC["消费接收数据<br/>OtaUart_Process()"]
  PROC --> READ["读取一个已缓存字节<br/>OtaPort_ReadByte()<br/>Esp32Uart_ReadByte()"]
  BUF -. 读取 .-> READ
  READ -->|返回字节到 Process| TYPE{"当前解析模式？"}
  TYPE -->|文本| LINE["拼成完整一行<br/>OtaUart_PushLineByte()"]
  LINE -->|遇到换行| CTRL["按命令分发<br/>OtaUart_HandleControlLine()"]
  TYPE -->|升级二进制数据| FRAME["按帧状态逐字节收集<br/>OtaUart_PushFrameByte()"]
  CTRL -->|OTA_BEGIN| BEG["校验开始参数<br/>OtaUart_HandleBegin()"]
  CTRL -->|STATUS 且未忙| STATUS["更新网络与升级状态<br/>OtaPort_HandleGatewayStatus()<br/>Esp32Status_ProcessLine()"]
  CTRL -->|OTA_END 且忙| FIN["整包校验并写 pending<br/>OtaApp_Finalize()"]
  CTRL -->|OTA_ABORT 且忙| AB["清除本次接收状态<br/>OtaApp_Abort()"]
  STATUS -. 状态快照 .-> UI["下次 UI 任务读取并绘制<br/>Esp32Status_Get() / AppUi_Render()"]
```

忙状态下，帧边界遇到字符 O 会切换为控制行接收，以识别 OTA_END / OTA_ABORT。STATUS 并不是忙状态解析器普遍支持的控制行，这一点不能从“有 sendStatus 函数”就推断其在整个传输期一定被显示。

## 图 6：开始、逐帧传输、结束的双芯片握手

以下时序图中的 STM32“串口解析层”均通过图 5 的 ISR 缓冲与 OTA 任务通道到达。

```mermaid
sequenceDiagram
  participant E as ESP32 下载与发送
  participant U as STM32 OtaUart 串口解析
  participant A as STM32 OtaApp 固件管理
  participant F as STM32 Flash
  E->>U: beginTransfer() 发送 OTA_BEGIN
  U->>U: HandleControlLine() → OtaUart_HandleBegin()
  U->>A: OtaApp_Begin(running, target, size, sha256)
  A->>A: OtaApp_TargetSlotIsAllowed() / OtaApp_IsSha256TextValid()
  A->>F: OtaApp_EraseTarget() → HAL_FLASHEx_Erase()
  A->>A: Sha256_Init()
  A-->>U: HAL_OK
  U-->>E: OtaUart_SendLine() 发 OTA_READY
  Note over E: waitForLine() 收到 OTA_READY 才继续
  loop 每帧最多 512 字节，直到整包传完
    E->>E: transfer() → readExactly() 读取 HTTP 流
    E->>E: sendFrame() → crc16() 打包序号、长度和校验
    E->>U: serial_.write() 发二进制帧
    U->>U: OtaUart_PushFrameByte() → OtaUart_Crc16()
    U->>U: 校验通过 → OtaUart_AcknowledgeFrame()
    U->>A: OtaApp_Write(offset, payload, length)
    A->>F: HAL_FLASH_Program() 半字写入目标槽
    A->>F: memcmp() 读回比较
    A->>A: Sha256_Update() / written_size 增加
    A-->>U: HAL_OK
    U-->>E: OtaUart_SendSequenceResponse() 发 OTA_ACK,序号
    E->>E: waitForLine() 接受 ACK 后推进下一帧
    E->>E: onTransferProgress() 更新本地进度，条件满足时上报
  end
  E->>U: endTransfer() 发送 OTA_END
  U->>A: OtaUart_HandleControlLine() → OtaApp_Finalize()
  A->>A: Sha256_Final() → Sha256_HexEquals()
  A->>A: Ota_VectorWordsAreValid() 检查新程序向量表
  A->>F: Ota_Crc32() 读取完整目标镜像计算 CRC32
  A->>A: OtaStorage_Load() → OtaRecord_Make()
  A->>F: OtaStorage_Save() 保存 active=A / pending=B
  A-->>U: HAL_OK
  U-->>E: OtaUart_SendLine() 发 OTA_OK
  U->>U: reset_pending=true
  Note over U: OtaUart_Process() 尾部执行待复位动作
  U->>U: OtaPort_ResetToBootloader() → NVIC_SystemReset()
  Note over E: endTransfer() 接受 OTA_OK 后返回成功
  E->>E: processPendingOta() 标记等待启动，reportProgress(99)
```

为缩短图中标签，HandleControlLine 指 `OtaUart_HandleControlLine()`；transfer、sendFrame、waitForLine 等是 `Stm32OtaLink` 成员函数。

`OtaUart_SendSequenceResponse()` → `OtaUart_SendLine()` → `OtaPort_Send()` → `HAL_UART_Transmit()` 是 ACK/NACK 的具体发送链。`OTA_READY`、`OTA_OK` 也使用同一行发送路径。

每帧 CRC16 检查串口数据；整包 SHA256 检查接收内容与通知摘要；Flash 中计算的 CRC32 被保存为 pending_crc32，供下次 Bootloader 复查。这里 SHA256 摘要比较不是公钥数字签名验证。

## 图 7：复位后为什么能进入新固件

```mermaid
flowchart TB
  RESET["STM32 软件复位"] --> B["再次运行 Bootloader<br/>boot_main.c: main()"]
  B --> SEL["决定本次启动目标<br/>Bootloader_SelectSlot()"]
  SEL --> LOAD["读取最新有效记录<br/>OtaStorage_Load()"]
  LOAD --> HAS{"有有效记录且有 pending？"}
  HAS -->|没有有效记录| FACT["仅尝试 Slot A<br/>Bootloader_VectorIsValid(A)"]
  HAS -->|记录有效但无 pending| OLD["检查已确认槽<br/>Bootloader_VectorIsValid(active)"]
  HAS -->|有 pending| VAL["检查新镜像<br/>Bootloader_PendingImageIsValid()<br/>OtaRecord_IsValid()<br/>Bootloader_VectorIsValid()<br/>Ota_Crc32()"]
  VAL --> OK{"镜像有效且 attempt_count 小于 2？"}
  OK -->|否| CLEAR["取消待启动状态<br/>Bootloader_ClearPending()<br/>OtaRecord_Make() / OtaStorage_Save()"]
  CLEAR --> OLD
  OK -->|是| CNT["先记录本次试启动<br/>OtaRecord_SetAttemptCount()<br/>OtaStorage_Save()"]
  CNT --> SAVED{"保存成功？"}
  SAVED -->|否| OLD
  SAVED -->|是| NEW["返回 pending 槽"]
  NEW --> J["由 main 发起跳转<br/>Bootloader_JumpToSlot()"]
  FACT -->|向量表有效| J
  OLD -->|向量表有效| J
  FACT -->|无效| STOP["返回 NONE；停在 Bootloader"]
  OLD -->|无效| STOP
  J --> APP["新应用启动<br/>Reset_Handler → main()<br/>再次创建并运行四个任务"]
```

“尝试两次”按复位后进入 Bootloader 的次数记账。并非到了十秒就由 Bootloader 自动抢回 CPU。若新程序卡死且没有看门狗或外部复位，Bootloader 不会自行再次运行。

## 图 8：新应用确认，云端才收到 100%

```mermaid
flowchart TB
  T["新应用 OTA 任务循环<br/>StartOtaTask()"] --> PROC["先处理串口<br/>OtaUart_Process()"]
  PROC -->|返回后| CHECK["检查是否需要确认<br/>RtosOta_ConfirmWhenStable(boot_time)"]
  CHECK --> REQUIRED["当前槽是否等于 pending<br/>OtaApp_IsConfirmationRequired()"]
  REQUIRED --> TIME{"需要确认且任务启动已满 10 秒？"}
  TIME -->|否| NEXT["osDelay(1)，下一轮再检查"]
  TIME -->|是| CONF["把当前槽设为 active，清除 pending<br/>OtaApp_ConfirmRunning()<br/>OtaRecord_Make() / OtaStorage_Save()"]
  CONF -->|保存成功| SEND["报告新程序已确认<br/>OtaPort_Send()<br/>BOOT_OK,版本号"]
  SEND -. 串口 .-> E["ESP32 读到启动确认<br/>processStm32Lines()<br/>Stm32OtaLink::readLine()"]
  E --> VER["更新当前版本<br/>HuaweiIotda::setCurrentVersion()"]
  VER --> MATCH{"正在等待启动且版本匹配目标？"}
  MATCH -->|是| DONE["上报安装完成<br/>HuaweiIotda::reportProgress(100)<br/>sendStatus(ota_ok)"]
  DONE --> VREP["报告当前版本<br/>HuaweiIotda::reportCurrentVersion()"]
  MATCH -->|否：不报本次升级成功| VREP
  DONE -. publishJson → MQTT .-> CLOUD["华为云接收升级完成报告"]
```

当前“稳定”判据只是：当前槽为 pending、OTA 任务运行到十秒检查点、元数据确认写入成功。没有逐一检查传感器、Wi-Fi 和每个任务是否健康。保存失败不会设置 confirmed=true，任务后面会继续尝试。

`BOOT_OK` 的发送使用 `OtaPort_Send()`，没有应用层确认重发；设备确认成功和云端成功收到报告仍是两件事。

## 状态怎样回到 LCD

`sendPeriodicStatus()` 每到两秒阈值调用 `sendStatus()`；另有开始、进度和结果路径主动调用它。

```mermaid
flowchart LR
  S["ESP32 状态文本<br/>sendStatus()"] -. STATUS .-> R["STM32 接收并分发<br/>OtaUart_Process()<br/>OtaUart_HandleControlLine()"]
  R --> P["解析并保存<br/>OtaPort_HandleGatewayStatus()<br/>Esp32Status_ProcessLine()"]
  P -. 状态快照 .-> UI["UI 任务读取<br/>Esp32Status_Get()"]
  UI --> DRAW["绘制 Wi-Fi / MQTT / OTA<br/>AppUi_Render()<br/>AppUi_DrawSystemStatus()"]
```

该图描述 STATUS 被成功接收、且解析器处于可处理 STATUS 状态时的路径。下载期间 `processPendingOta()` 同步占用 ESP32 loop；`transfer()` / `waitForLine()` 内没有调用 `huawei.update()`，因此不能画成下载时主循环仍持续执行 MQTT 保活。进度回调会发送 STATUS，但 STM32 忙状态并不正常分发 STATUS；当前 LCD 进度不应被描述成保证连续实时更新。

## 关键辅助函数在做什么

| 所在模块 | 函数组 | 作用与主线位置 |
|---|---|---|
| DHT11 | Dht11_SetOutput / Dht11_SetInput / Dht11_DelayUs / Dht11_WaitFor / Dht11_CyclesPerUs | 为 Dht11_Read 提供引脚方向切换、微秒时序和电平等待 |
| BH1750 | Bh1750_Init / Bh1750_ReadLux | 分别发送初始化命令、通过 HAL_I2C_Master_Receive 读取并换算光照 |
| MQ135 | Mq135_Read | HAL_ADC_Start → HAL_ADC_PollForConversion → HAL_ADC_GetValue → HAL_ADC_Stop |
| UI 固定布局 | AppUi_DrawEnvironmentLayout / AppUi_DrawSystemStatusPage | 在首次进入或切页时绘制固定内容 |
| UI 绘制辅助 | AppUi_DrawHeader / AppUi_DrawFooter / AppUi_DrawCard / AppUi_DrawDashboardStatus / AppUi_DrawProgressBar | 绘制标题、底部提示、卡片、网络摘要和进度 |
| UI 数值辅助 | AppUi_FormatTemperature / AppUi_WriteValueOrNoData / AppUi_WriteCardValue | 格式化温度，根据 valid 决定显示数值或 NO DATA |
| UI 底层桥接 | AppUi_FillRect / AppUi_WriteText | 转成 LCD_SetArea、LCD_WriteCmd、LCD_WriteData 和 LCD_WriteAsciiString |
| LCD 文本 | LCD_WriteAsciiString / LCD_WriteAsciiChar | 字符串拆成字符，按字库写像素 |
| LCD 图形 | LCD_ClearAll / LCD_DrawRectangle / LCD_DrawLine / LCD_DrawPoint | 页面清屏与框线像素绘制；最终通过 FSMC 地址写屏 |
| 串口帧辅助 | OtaUart_ResetFrame / OtaUart_WriteLe16 / OtaUart_Crc16 | 重置帧接收状态、小端编码、计算 CRC16 |
| 串口参数检查 | OtaUart_VersionIsSafe / OtaUart_NormalizeSha256 | 校验版本字符、检查并统一 SHA256 大小写 |
| 串口状态 | OtaUart_SetMessage / OtaUart_IsBusy | 保存接收层状态、供上传任务判断是否暂停 ENV |
| SHA256 | Sha256_Init / Sha256_Update / Sha256_Final / Sha256_HexEquals | 开始摘要、逐帧累计、得出整包摘要、比较目标值 |
| SHA256 内部 | Sha256_ProcessBlock / Sha256_RotateRight | 64 字节块压缩和位运算，服务上述摘要函数 |
| 记录构造 | OtaRecord_Make / OtaRecord_SetAttemptCount | 生成记录或修改次数，并重新计算记录 CRC |
| 记录检查 | OtaRecord_IsValid / OtaRecord_IsApplicationSlot / OtaRecord_CalculateCrc | 字段组合、槽位、记录 CRC 检查 |
| 记录选择 | OtaRecord_SelectNewest | 从两个有效副本中选新记录；由 Load 和 Save 使用 |
| Flash 记录 | OtaStorage_RecordAt / OtaStorage_ProgramRecord | 把地址解释成记录、按半字写入记录；服务 Load/Save |
| 完整性 | Ota_ImageSizeIsValid / Ota_VectorWordsAreValid / Ota_Crc32 | 镜像大小、启动入口和栈范围、镜像或记录 CRC |
| 网关错误 | Stm32OtaLink::lastErrorText / otaResultCodeForFailure | 取得失败点并映射云端结果码，由 processPendingOta 上报 |
| 网关状态读取 | HuaweiIotda::wifiConnected / mqttConnected / lastPublishOk | sendStatus 获取联网和上报状态 |

## 三个成功回执的区别

| 消息 | 谁发出 | 真正含义 |
|---|---|---|
| OTA_ACK,n | 旧应用中的串口解析层 | 第 n 帧已被接受并写入目标槽 |
| OTA_OK | 旧应用中的串口解析层 | 整包校验成功，pending 已保存，即将复位 |
| BOOT_OK,version | 新应用中的 OTA 任务 | 新槽已完成当前代码的十秒确认并设为 active |

ESP32 只有在“正在等待启动，且 BOOT_OK 版本等于本次目标版本”时才发送本次 100% 报告。

## 失败路径和源码注释容易误导的地方

1. 帧 CRC 错误或序号/Flash 写入不符合条件：STM32 发 OTA_NACK；ESP32 同一帧最多发送三次（包含第一次）。超过次数，transfer 返回失败，downloadAndTransfer 调用 abort 发送 OTA_ABORT。
2. ACK 丢失不等于可以无条件恢复：STM32 已推进序号后，重发旧帧会因序号不同收到 NACK，当前接收端没有重复帧重新 ACK 的逻辑。
3. OtaApp_Finalize 失败：返回 OTA_ERROR,verify，不启动新槽。旧 active 保留。
4. Bootloader 校验 pending 失败或两次试启动用尽：清 pending，尝试原 active；原 active 也无效则停机。清 pending 的保存结果目前被忽略。
5. ota_cloud.c 中的 OtaCloud_ParseEvent 虽在工程文件清单中，但当前 Arduino 网关主线不调用它。真正解析云端 JSON 的是 ESP32 的 HuaweiIotda::handleMqttMessage，不能因文件名把接收入口画错。
6. OtaRecord_BootSlot 是纯状态辅助函数；当前 Bootloader 实际选择路径是 Bootloader_SelectSlot。
7. 当前复位链是 OtaPort_ResetToBootloader，不是另一个存在的 OtaApp_ResetToBootloader。测试辅助 OtaUart_BuildFrameForTest / OtaUart_FrameIsValidForTest 也不属于设备正常接收路径。
8. app_ui.c 还留有“未来创建 OTA 任务”的旧注释，但 freertos.c 已实际创建 StartOtaTask；图以执行代码为准。
9. boot_main.c 注释称 CRC32 由云端提供，但执行代码中 pending_crc32 来自 OtaApp_Finalize 对目标 Flash 的计算。
10. OTA_UART 接收缓冲区为 256 字节，固件帧负载最多 512 字节；其正常工作依靠任务持续消费，并非整帧一次装进环形缓冲区。本文不把静态流程核对当成吞吐或掉电测试。

## 对照源码阅读顺序

1. `Core/Src/main.c`：应用的初始化顺序。
2. `Core/Src/freertos.c`：四任务及 RtosData / RtosOta 辅助函数。
3. `Core/Src/env_sensor.c`、`app_ui.c`、`esp32_uart.c`、`esp32_status.c`：日常数据路径。
4. `portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway/huawei_iotda.cpp`：云端指令入口。
5. 同目录 `esp32c3_ota_gateway.ino`：主循环、下载升级事务、启动确认上报。
6. 同目录 `stm32_ota_link.cpp`：开始、帧传输和结束握手。
7. `user/OTA/Src/ota_uart.c`：STM32 串口协议分发。
8. `user/OTA/Src/ota_app.c`：擦除、写入、整包验证和确认。
9. `user/OTA/Src/ota_storage.c`、`ota_metadata.c`、`ota_integrity.c`、`sha256.c`：持久化与完整性支持。
10. `portable_ota/stm32/stm32f103_example/bootloader/boot_main.c`：复位后的槽位选择和跳转。

上面 Core、user 路径以 huanjing_HAL 为基准，portable_ota 以工作区根目录为基准。Bootloader 工程引用 portable_ota/stm32/ota_core 支持文件；应用工程引用 huanjing_HAL/user/OTA。此图以读取到的实现为准，不更改固件代码。
