/*
 * ESP32-C3 Arduino 网关：
 * 1. 接收 STM32 的环境数据并上传华为云；
 * 2. 接收华为云 STM32 OTA 任务；
 * 3. 下载 bin 后用 UART1 可靠传给 STM32 的非活动槽位。
 *
 * 主循环的数据流：IoTDA MQTT 下行 -> pendingPackage -> HTTPS 数据流 ->
 * 512 字节 UART 帧 -> STM32 Flash；反向状态流是 STM32 ENV/BOOT_OK -> 云端。
 * 传输完成只报告 99%，直到收到新应用发出的版本匹配 BOOT_OK 才报告 100%。
 */
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "ota_gateway_config.h"
#include "huawei_iotda.h"

constexpr int STM32_RX_PIN = OTA_STM32_RX_PIN;
constexpr int STM32_TX_PIN = OTA_STM32_TX_PIN;

HardwareSerial Stm32Serial(1);
Stm32OtaLink stm32Link(Stm32Serial);
HuaweiIotda huawei;

bool otaActive = false;
uint8_t otaProgress = 0U;       /* 已可靠传给 STM32 的近似百分比。 */
uint32_t lastStatusMs = 0U;    /* 周期 STATUS 上报的节流时间戳。 */
String otaTargetVersion;       /* 当前事务期望最终启动的版本。 */
bool otaWaitingForBoot = false;/* 传输已结束，等待新应用确认启动。 */

void sendStatus(const char *message)
{
  Stm32Serial.printf("STATUS,%u,%u,%u,%u,%u,%s\r\n",
                     huawei.wifiConnected() ? 1U : 0U,
                     huawei.mqttConnected() ? 1U : 0U,
                     huawei.lastPublishOk() ? 1U : 0U,
                     otaActive ? 1U : 0U, otaProgress, message);
  /* COM5 调试信息只显示连接状态，不包含 Wi-Fi 密码、设备密钥或访问令牌。 */
  Serial.printf("[Gateway] WiFi=%u MQTT=%u OTA=%u %u%% %s\n",
                huawei.wifiConnected() ? 1U : 0U,
                huawei.mqttConnected() ? 1U : 0U,
                otaActive ? 1U : 0U, otaProgress, message);
}

void onTransferProgress(uint8_t progress)
{
  otaProgress = progress;
  if ((progress % 10U) == 0U)
  {
    /* result_code=0 表示该阶段正常进行，不表示升级已经完成。 */
    huawei.reportProgress(otaTargetVersion, progress, 0, "downloading");
    sendStatus("downloading");
  }
}

/* 将本地失败点转换成华为云定义的标准 OTA 结果码。 */
int otaResultCodeForFailure(const String &reason)
{
  if (reason == "slot") return 8;                 /* 不支持的升级包类型。 */
  if ((reason == "file_size") || (reason == "stm32_verify")) return 7;
  if ((reason == "http_begin") || (reason == "http_get") || (reason == "http_read")) return 6;
  if ((reason == "stm32_ready") || (reason == "frame_ack") ||
      (reason == "frame_data")) return 10;        /* STM32 安装阶段失败。 */
  return 255;                                      /* 未归类的内部异常。 */
}

bool downloadAndTransfer(const Stm32OtaPackage &package, String &failureReason)
{
  HTTPClient http;
  WiFiClientSecure secureClient;
  bool started;
  int code;

  if (package.targetSlot != 'A' && package.targetSlot != 'B')
  {
    failureReason = "slot";
    return false;
  }
  /* 先验证目标槽，再创建网络请求，明显错误不会触碰 STM32。 */
  if (package.url.startsWith("https://"))
  {
    /* IoTDA 下载 URL 使用 HTTPS；后续可在此替换为固定根证书。 */
    secureClient.setInsecure();
    started = http.begin(secureClient, package.url);
  }
  else
  {
    started = http.begin(package.url);
  }
  if (!started)
  {
    failureReason = "http_begin";
    return false;
  }

  if (package.needsAuthorization)
  {
    http.addHeader("Authorization", "Bearer " + package.accessToken);
  }
  code = http.GET();
  if (code != HTTP_CODE_OK)
  {
    failureReason = "http_get";
    http.end();
    return false;
  }
  if ((http.getSize() > 0) && (static_cast<uint32_t>(http.getSize()) != package.fileSize))
  {
    failureReason = "file_size";
    http.end();
    return false;
  }
  /* HTTP 已准备好之后才让 STM32 擦除目标槽，缩短槽位处于半写状态的时间。 */
  if (!stm32Link.beginTransfer(package))
  {
    failureReason = stm32Link.lastErrorText(); /* 未收到 OTA_READY 时会显示 stm32_ready。 */
    http.end();
    return false;
  }
  if (!stm32Link.transfer(http.getStream(), package.fileSize, onTransferProgress))
  {
    failureReason = stm32Link.lastErrorText(); /* 例如 frame_ack 或 http_read。 */
    stm32Link.abort();
    http.end();
    return false;
  }
  http.end();
  if (!stm32Link.endTransfer())
  {
    failureReason = stm32Link.lastErrorText();
    return false;
  }
  return true;
}

void processStm32Lines()
{
  String line;
  while (stm32Link.readLine(line))
  {
    if (line.startsWith("ENV,"))
    {
      huawei.publishEnvironment(line);
    }
    else if (line.startsWith("BOOT_OK,"))
    {
      String version = line.substring(String("BOOT_OK,").length());
      huawei.setCurrentVersion(version);
      /* 只有 Bootloader 已切换且新应用已启动，才允许向云端上报 100%。 */
      if (otaWaitingForBoot && (version == otaTargetVersion))
      {
        huawei.reportProgress(version, 100U, 0, "installed");
        otaWaitingForBoot = false;
        sendStatus("ota_ok");
      }
      huawei.reportCurrentVersion();
    }
  }
}

/*
 * 执行一笔完整的待处理升级事务。
 * 99% 只表示固件已经传完并等待 STM32 重启；只有收到版本匹配的 BOOT_OK，
 * processStm32Lines() 才会向云端报告 100%，防止把“传输成功”误报成“安装成功”。
 */
void processPendingOta()
{
  Stm32OtaPackage package;

  if (otaActive || !huawei.takePendingPackage(package))
  {
    return;
  }

  String failureReason = "unknown";
  otaActive = true;
  otaProgress = 0U;
  otaTargetVersion = package.version;
  sendStatus("ota_start");

  if (downloadAndTransfer(package, failureReason))
  {
    otaProgress = 99U;
    otaWaitingForBoot = true;
    huawei.reportProgress(package.version, 99U, 0, "transferred_rebooting");
    sendStatus("rebooting");
  }
  else
  {
    huawei.reportProgress(package.version, otaProgress,
                          otaResultCodeForFailure(failureReason), failureReason);
    sendStatus(failureReason.c_str());
  }

  otaActive = false;
}

/* 每两秒给 STM32 一份状态快照；无须让主循环承担计时分支细节。 */
void sendPeriodicStatus()
{
  if ((millis() - lastStatusMs) < 2000U)
  {
    return;
  }

  lastStatusMs = millis();
  sendStatus(otaActive ? "downloading" : "ready");
}

void setup()
{
  Serial.begin(115200);       /* COM5：只用于烧录后的 USB 调试日志。 */
  delay(200U);
  stm32Link.begin(STM32_RX_PIN, STM32_TX_PIN, OTA_STM32_BAUD_RATE);
  huawei.begin();
  sendStatus("started");
}

void loop()
{
  /* 每个步骤自行快速返回；顺序保证先维护网络，再消费状态和升级任务。 */
  huawei.update();
  processStm32Lines();
  processPendingOta();
  sendPeriodicStatus();
}
