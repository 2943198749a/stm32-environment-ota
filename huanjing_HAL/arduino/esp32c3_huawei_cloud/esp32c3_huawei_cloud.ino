/*
 * ESP32-C3 Arduino 程序：接收 STM32 环境数据并上传华为云。
 *
 * STM32 发来的格式：ENV,25.3,60.1,350,42\r\n
 * 接线：STM32 PA2(TX) -> ESP32-C3 GPIO20(U0RXD)
 *       STM32 PA3(RX) <- ESP32-C3 GPIO21(U0TXD)
 *
 * Arduino IDE 需要安装 PubSubClient 库。
 *
 * 数据流：STM32 ENV 文本 -> Serial0 行缓冲 -> JSON -> MQTT 属性上报；
 * 状态流：Wi-Fi/MQTT/发布结果 -> STATUS 文本 -> STM32 LCD。
 * keepWiFiConnected、keepMqttConnected 和 readStm32Line 都是短轮询函数，
 * loop() 不使用长时间阻塞，才能同时维护网络心跳和串口接收。
 */

/* ESP32 的 Wi-Fi 功能。 */
#include <WiFi.h>
/* 简单 MQTT 客户端库。 */
#include <PubSubClient.h>

/*
 * Wi-Fi 名称和密码：程序不会在串口打印它们。
 * 当前文件把凭据编译进固件，仅适合本地实验；分享工程前应替换，并优先改成
 * 单独的、不提交版本库的本地配置文件。注释不会让固件中的明文凭据变安全。
 */
const char *WIFI_SSID = "CHANGE_ME";
const char *WIFI_PASSWORD = "CHANGE_ME";

/* 华为云设备连接参数。 */
const char *HUAWEI_HOST = "CHANGE_ME";
const uint16_t HUAWEI_PORT = 1883;
const char *HUAWEI_DEVICE_ID = "CHANGE_ME";
const char *HUAWEI_CLIENT_ID = "CHANGE_ME";
const char *HUAWEI_PASSWORD = "CHANGE_ME";
const char *HUAWEI_TOPIC = "CHANGE_ME";

/* 板载 ESP32-C3 的 U0RXD/U0TXD 引脚。 */
const int UART0_RX_PIN = 20;
const int UART0_TX_PIN = 21;

/* WiFiClient 负责 TCP，PubSubClient 在 TCP 上实现 MQTT。 */
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

/* 保存从 STM32 收到的一行文本。 */
char stm32Line[96];
size_t stm32LineLength = 0U;

/* 限制反复断线时的重连频率，避免不停刷屏。 */
unsigned long lastWiFiAttemptMs = 0U;
unsigned long lastMqttAttemptMs = 0U;

/* 记录最近一次发布是否成功，返回给 STM32 的 LCD 页面。 */
bool lastPublishOk = false;

/* 向 STM32 返回当前 Wi-Fi、MQTT 和发布状态。 */
void sendStatus()
{
  /* bool 转成 0/1 文本，STM32 可用固定格式 sscanf 安全解析。 */
  uint8_t wifiOk = (WiFi.status() == WL_CONNECTED) ? 1U : 0U;
  uint8_t mqttOk = mqttClient.connected() ? 1U : 0U;
  uint8_t publishOk = lastPublishOk ? 1U : 0U;

  /* Serial0 对应 GPIO20/GPIO21，会送回 STM32 的 PA3。 */
  Serial0.printf("STATUS,%u,%u,%u\r\n", wifiOk, mqttOk, publishOk);
}

/* Wi-Fi 未连接时，每 5 秒发起一次非阻塞连接。 */
void keepWiFiConnected()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  if (millis() - lastWiFiAttemptMs < 5000U)
  {
    return;
  }

  lastWiFiAttemptMs = millis();
  lastPublishOk = false;
  Serial.println("[WiFi] Connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  sendStatus();
}

/* MQTT 未连接时，每 5 秒尝试重新连接华为云。 */
void keepMqttConnected()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (mqttClient.connected())
  {
    return;
  }

  if (millis() - lastMqttAttemptMs < 5000U)
  {
    return;
  }

  lastMqttAttemptMs = millis();
  lastPublishOk = false;
  Serial.println("[MQTT] Connecting Huawei Cloud...");

  /* 参数依次为：客户端 ID、用户名（设备 ID）、密码。 */
  if (mqttClient.connect(HUAWEI_CLIENT_ID, HUAWEI_DEVICE_ID, HUAWEI_PASSWORD))
  {
    Serial.println("[MQTT] Connected.");
  }
  else
  {
    Serial.print("[MQTT] Failed, state=");
    Serial.println(mqttClient.state());
  }

  sendStatus();
}

/* 将当前收到的四项环境数据包装成华为云属性上报 JSON。 */
void publishEnvironment()
{
  float temperature = 0.0F;
  float humidity = 0.0F;
  uint16_t light = 0U;
  uint16_t airQuality = 0U;
  char payload[220];

  /* 只接受完整的 ENV 数据行，格式错误时不会上传。 */
  if (sscanf(stm32Line, "ENV,%f,%f,%hu,%hu", &temperature, &humidity, &light, &airQuality) != 4)
  {
    Serial.print("[UART] Invalid line: ");
    Serial.println(stm32Line);
    return;
  }

  /* 没有 MQTT 连接时，保留数据由下一次 STM32 发送时再上传。 */
  if (!mqttClient.connected())
  {
    Serial.println("[MQTT] Offline, ENV data skipped.");
    lastPublishOk = false;
    sendStatus();
    return;
  }

  /* service_id 必须与华为云产品模型中的服务标识一致。 */
  /* snprintf 最多写 sizeof(payload)-1 字节，并自动补字符串结束符。 */
  snprintf(payload, sizeof(payload),
           "{\"services\":[{\"service_id\":\"environment\",\"properties\":{\"temperature\":%.1f,\"humidity\":%.1f,\"light\":%u,\"air_quality\":%u}}]}",
           temperature, humidity, light, airQuality);

  Serial.print("[UART] ENV: ");
  Serial.println(stm32Line);

  /* 成功返回 true，表示 MQTT 客户端已经把数据交给网络栈。 */
  lastPublishOk = mqttClient.publish(HUAWEI_TOPIC, payload);
  Serial.println(lastPublishOk ? "[MQTT] Publish OK." : "[MQTT] Publish failed.");
  sendStatus();
}

/* 逐个字符读取 STM32 串口；收到换行符时处理完整的一行数据。 */
void readStm32Line()
{
  /* 串口可能一次只到几个字符，因此 stm32LineLength 必须跨 loop 保留。 */
  while (Serial0.available() > 0)
  {
    char received = (char)Serial0.read();

    if (received == '\n')
    {
      stm32Line[stm32LineLength] = '\0';
      publishEnvironment();
      stm32LineLength = 0U;
      continue;
    }

    /* \r 是换行的一部分，不需要保存。 */
    if (received == '\r')
    {
      continue;
    }

    /* 数组还有空间才保存新字符，避免越界。 */
    if (stm32LineLength < sizeof(stm32Line) - 1U)
    {
      stm32Line[stm32LineLength++] = received;
    }
    else
    {
      /* 数据过长时直接丢弃本行，等待下一行重新同步。 */
      stm32LineLength = 0U;
    }
  }
}

/* Arduino 上电后只初始化一次。 */
void setup()
{
  /* Serial 是 ESP32-C3 原生 USB 调试串口。 */
  Serial.begin(115200);
  delay(300);
  Serial.println("ESP32-C3 Huawei Cloud bridge started.");

  /* Serial0 是接到 STM32 PA2/PA3 的硬件串口。 */
  Serial0.begin(115200, SERIAL_8N1, UART0_RX_PIN, UART0_TX_PIN);

  WiFi.mode(WIFI_STA);
  mqttClient.setServer(HUAWEI_HOST, HUAWEI_PORT);
  mqttClient.setBufferSize(256U);
  sendStatus();
}

/* Arduino 主循环：维护网络、处理 MQTT、读取 STM32 数据。 */
void loop()
{
  keepWiFiConnected();
  keepMqttConnected();
  mqttClient.loop();
  readStm32Line();
}
