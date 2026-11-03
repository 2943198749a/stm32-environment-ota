# ESP32-C3 Arduino MQTT Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make ESP32-C3 run an Arduino MQTT bridge that receives STM32 environment data over UART0 and uploads it to Huawei Cloud.

**Architecture:** STM32 keeps sensor simulation, keys, and LCD work, but USART2 only sends `ENV,temperature,humidity,light,air\r\n`. ESP32-C3 uses Arduino `Serial0` to parse that line, reconnect Wi-Fi/MQTT when needed, and sends the Huawei Cloud property-report JSON through PubSubClient.

**Tech Stack:** STM32Cube HAL/C, Arduino IDE, ESP32 Arduino core, WiFi.h, PubSubClient, mbedTLS SHA-256/HMAC.

## Global Constraints

- STM32 USART2 remains at the board's existing 115200 baud setting and uses PA2 TX to ESP32-C3 U0RXD.
- ESP32-C3 USB serial remains the Arduino debug serial; no ESP-AT firmware or AT commands are used.
- UART packet format is ASCII `ENV,<temperature>,<humidity>,<light>,<air>\r\n`.
- Arduino source contains Chinese beginner-oriented comments and never prints Wi-Fi or device secrets.

---

### Task 1: Define and test the STM32-to-ESP32 UART packet

**Files:**
- Modify: `D:\aiguigu\huanjing_HAL\Core\Src\esp32_c3.c`
- Modify: `D:\aiguigu\huanjing_HAL\Core\Inc\esp32_c3.h`
- Modify: `D:\aiguigu\huanjing_HAL\tests\test_esp32_at_receive.ps1`

**Interfaces:**
- Consumes: `const EnvData_t *data` from `env_sensor.h`.
- Produces: `Esp32C3_RequestUpload(const EnvData_t *data)`, which transmits one `ENV` line on `huart2`.

- [ ] **Step 1: Write the failing test**

```powershell
if ($source -notmatch 'ENV,%ld.%ld,%lu.%lu,%u,%u\\r\\n') {
    throw 'STM32 must send the documented ENV UART packet.'
}
if ($source -match 'AT\\+MQTT|AT\\+CWJAP') {
    throw 'STM32 bridge code must not contain ESP-AT commands.'
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `& .\tests\test_esp32_at_receive.ps1`

Expected: FAIL because the current source still contains ESP-AT MQTT commands.

- [ ] **Step 3: Write minimal implementation**

```c
(void)snprintf(packet, sizeof(packet),
               "ENV,%ld.%ld,%lu.%lu,%u,%u\r\n",
               (long)temperature_integer, (long)temperature_decimal,
               (unsigned long)humidity_integer, (unsigned long)humidity_decimal,
               data->light_lux, data->air_ppm);
(void)HAL_UART_Transmit(&huart2, (uint8_t *)packet,
                        (uint16_t)strlen(packet), 200U);
```

- [ ] **Step 4: Run test and STM32 build**

Run: `& .\tests\test_esp32_at_receive.ps1; & 'C:\Keil_v5\UV4\UV4.exe' -b .\MDK-ARM\42_lcd_hal.uvprojx`

Expected: packet test passes and the build log reports `0 Error(s), 0 Warning(s)`.

### Task 2: Create and test the ESP32-C3 Arduino bridge

**Files:**
- Create: `D:\aiguigu\huanjing_HAL\arduino\esp32c3_huawei_cloud\esp32c3_huawei_cloud.ino`
- Modify: `D:\aiguigu\huanjing_HAL\tests\test_esp32_at_receive.ps1`

**Interfaces:**
- Consumes: UART lines beginning `ENV,`.
- Produces: `publishEnvironment()` publishing the IoTDA `environment` service properties to the device report topic.

- [ ] **Step 1: Write the failing source check**

```powershell
if (-not (Test-Path "$PSScriptRoot\..\arduino\esp32c3_huawei_cloud\esp32c3_huawei_cloud.ino")) {
    throw 'Arduino bridge sketch is missing.'
}
if ($arduinoSource -notmatch 'void publishEnvironment\(\)') {
    throw 'Arduino bridge must publish received sensor values.'
}
if ($arduinoSource -notmatch 'ENV,%f,%f,%hu,%hu') {
    throw 'Arduino bridge must parse the STM32 ENV packet.'
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `& .\tests\test_esp32_at_receive.ps1`

Expected: FAIL because the Arduino sketch does not yet exist.

- [ ] **Step 3: Write minimal Arduino implementation**

```cpp
void loop() {
  keepWiFiConnected();
  keepMqttConnected();
  mqttClient.loop();
  readStm32Line();
}
```

The sketch includes `WiFi.h`, `PubSubClient.h`, and `mbedtls/md.h`; uses UART0 pins that match board U0RXD/U0TXD; builds the client ID and HMAC password; and prints only state and sensor values to USB Serial.

- [ ] **Step 4: Run source checks**

Run: `& .\tests\test_esp32_at_receive.ps1`

Expected: PASS with `ESP32 Arduino bridge check passed.`

### Task 3: Document flashing and validation

**Files:**
- Modify: `D:\aiguigu\huanjing_HAL\README.md`

**Interfaces:**
- Consumes: completed STM32 hex and Arduino sketch.
- Produces: beginner instructions for Arduino IDE board selection, PubSubClient installation, serial monitor, and Huawei Cloud verification.

- [ ] **Step 1: Add exact validation sequence**

```markdown
1. Arduino IDE 选择 ESP32C3 Dev Module，USB CDC On Boot 设为 Enabled。
2. 安装 PubSubClient 库后烧录 esp32c3_huawei_cloud.ino。
3. 打开 ESP32 的 USB 串口监视器，波特率 115200。
4. 烧录 STM32 最新 hex；ESP32 依次显示 Wi-Fi、MQTT 和 ENV 数据上报结果。
```

- [ ] **Step 2: Verify all checks and build output**

Run: `& .\tests\test_esp32_at_receive.ps1; & 'C:\Keil_v5\UV4\UV4.exe' -b .\MDK-ARM\42_lcd_hal.uvprojx`

Expected: source check passes; STM32 build log contains `0 Error(s), 0 Warning(s)`.
