/*
 * ESP32-C3 到华为云 IoTDA 的会话实现。
 *
 * update() 是主循环唯一需要周期调用的入口：它用时间戳节流 Wi-Fi/MQTT
 * 重连，并持续执行 MQTT 心跳与消息回调。收到 OTA 下行消息后，本类只解析
 * 和保存包描述，不在 MQTT 回调里做耗时的 HTTP 下载或串口传输；主循环稍后
 * 通过 takePendingPackage() 取走任务。这样网络回调能尽快返回。
 */
#include <ArduinoJson.h>
#include "huawei_iotda.h"
#include "ota_gateway_config.h"

/* PubSubClient 只接受普通函数回调，用静态指针转发到当前对象。 */
HuaweiIotda *HuaweiIotda::instance_ = nullptr;

HuaweiIotda::HuaweiIotda() : mqttClient_(wifiClient_), packagePending_(false),
  lastPublishOk_(false), wifiConnecting_(false), currentVersion_(OTA_STM32_FIRMWARE_VERSION),
  lastWifiAttemptMs_(0U), lastMqttAttemptMs_(0U)
{
  /* 成员在初始化列表中一次性建立确定状态，构造函数不访问网络。 */
}

/* 配置工作模式和 MQTT 参数；真正连接由 update() 在主循环中完成。 */
void HuaweiIotda::begin()
{
  instance_ = this;
  WiFi.mode(WIFI_STA);
  mqttClient_.setServer(OTA_IOTDA_HOST, 1883U);
  /* OTA URL 和 access_token 较长，默认 256 字节 MQTT 缓冲区不够。 */
  mqttClient_.setBufferSize(6144U);
  mqttClient_.setKeepAlive(60U);
  mqttClient_.setCallback(HuaweiIotda::mqttCallback);
  /* 上电后立即允许第一次 Wi-Fi 连接；后续失败才按 20 秒间隔重试。 */
  lastWifiAttemptMs_ = millis() - 20000U;
}

void HuaweiIotda::ensureWifi()
{
  /* 已连接、正在连接、等待重试三个状态都快速返回，避免阻塞 Arduino loop。 */
  if (WiFi.status() == WL_CONNECTED)
  {
    wifiConnecting_ = false;
    return;
  }
  /* WL_IDLE_STATUS 表示上一条 WiFi.begin() 仍在执行，不能重复调用。 */
  if (WiFi.status() == WL_IDLE_STATUS)
  {
    return;
  }
  if (wifiConnecting_)
  {
    /* 有些 C3 固件在连接中仍返回 WL_DISCONNECTED；用本地状态避免重复 begin。 */
    if ((millis() - lastWifiAttemptMs_) < 30000U)
    {
      return;
    }
    WiFi.disconnect(false, false);
    wifiConnecting_ = false;
    return;
  }
  if ((millis() - lastWifiAttemptMs_) < 20000U)
  {
    return;
  }
  lastWifiAttemptMs_ = millis();
  WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
  wifiConnecting_ = true;
}

void HuaweiIotda::ensureMqtt()
{
  String downTopic;

  /* MQTT 建立在 Wi-Fi 之上；Wi-Fi 不通时没有必要尝试云端连接。 */
  if ((WiFi.status() != WL_CONNECTED) || mqttClient_.connected())
  {
    return;
  }
  if ((millis() - lastMqttAttemptMs_) < 5000U)
  {
    return;
  }
  lastMqttAttemptMs_ = millis();
  if (mqttClient_.connect(OTA_IOTDA_CLIENT_ID, OTA_IOTDA_DEVICE_ID, OTA_IOTDA_PASSWORD))
  {
    downTopic = "$oc/devices/" + String(OTA_IOTDA_DEVICE_ID) + "/sys/events/down";
    mqttClient_.subscribe(downTopic.c_str());
    reportCurrentVersion();
  }
}

void HuaweiIotda::update()
{
  /* 三步均为短操作，loop() 才能继续处理 STM32 串口和 OTA 传输。 */
  ensureWifi();
  ensureMqtt();
  if (mqttClient_.connected())
  {
    mqttClient_.loop();
  }
}

bool HuaweiIotda::publishJson(const String &topic, const String &json)
{
  if (!mqttClient_.connected())
  {
    return false;
  }
  return mqttClient_.publish(topic.c_str(), json.c_str());
}

bool HuaweiIotda::publishEnvironment(const String &envLine)
{
  float temperature;
  float humidity;
  unsigned int light;
  unsigned int air;
  StaticJsonDocument<320> document;
  String payload;
  String topic;

  /* 串口文本必须完整匹配四个测量值，半行或脏数据不会上传。 */
  if (sscanf(envLine.c_str(), "ENV,%f,%f,%u,%u", &temperature, &humidity,
             &light, &air) != 4)
  {
    lastPublishOk_ = false;
    return false;
  }

  JsonObject service = document.createNestedArray("services").createNestedObject();
  service["service_id"] = "environment";
  JsonObject properties = service.createNestedObject("properties");
  properties["temperature"] = temperature;
  properties["humidity"] = humidity;
  properties["light"] = light;
  properties["air_quality"] = air;
  serializeJson(document, payload);
  topic = "$oc/devices/" + String(OTA_IOTDA_DEVICE_ID) + "/sys/properties/report";
  lastPublishOk_ = publishJson(topic, payload);
  return lastPublishOk_;
}

void HuaweiIotda::reportCurrentVersion()
{
  StaticJsonDocument<256> document;
  String payload;
  String topic;
  JsonObject service = document.createNestedArray("services").createNestedObject();
  service["service_id"] = "$ota";
  service["event_type"] = "version_report";
  JsonObject paras = service.createNestedObject("paras");
  paras["sw_version"] = currentVersion_;
  paras["fw_version"] = currentVersion_;
  serializeJson(document, payload);
  topic = "$oc/devices/" + String(OTA_IOTDA_DEVICE_ID) + "/sys/events/up";
  publishJson(topic, payload);
}

void HuaweiIotda::reportProgress(const String &version, uint8_t progress,
                                 int resultCode, const String &description)
{
  StaticJsonDocument<256> document;
  String payload;
  String topic;
  JsonObject service = document.createNestedArray("services").createNestedObject();
  service["service_id"] = "$ota";
  service["event_type"] = "upgrade_progress_report";
  JsonObject paras = service.createNestedObject("paras");
  paras["version"] = version;
  paras["progress"] = progress;
  paras["result_code"] = resultCode;
  /* 华为云的可选字段：把实际失败点带回控制台，避免只看到笼统的失败。 */
  if (description.length() > 0U)
  {
    paras["description"] = description;
  }
  serializeJson(document, payload);
  topic = "$oc/devices/" + String(OTA_IOTDA_DEVICE_ID) + "/sys/events/up";
  publishJson(topic, payload);
}

void HuaweiIotda::setCurrentVersion(const String &version)
{
  if (version.length() > 0U)
  {
    currentVersion_ = version;
  }
}

bool HuaweiIotda::takePendingPackage(Stm32OtaPackage &package)
{
  if (!packagePending_)
  {
    return false;
  }
  package = pendingPackage_;
  packagePending_ = false;
  return true;
}

bool HuaweiIotda::wifiConnected() { return WiFi.status() == WL_CONNECTED; }
bool HuaweiIotda::mqttConnected() { return mqttClient_.connected(); }
bool HuaweiIotda::lastPublishOk() { return lastPublishOk_; }

void HuaweiIotda::mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  (void)topic;
  /* 静态回调不做业务，只把原始负载转给当前 HuaweiIotda 对象。 */
  if (instance_ != nullptr)
  {
    instance_->handleMqttMessage(payload, length);
  }
}

void HuaweiIotda::handleMqttMessage(const uint8_t *payload, unsigned int length)
{
  DynamicJsonDocument document(6144U);
  DeserializationError error = deserializeJson(document, payload, length);
  JsonObject service;
  const char *serviceId;
  const char *eventType;
  JsonObject paras;

  /* 先验证最外层 services 数组，避免对格式错误的 JSON 继续取字段。 */
  if (error || !document["services"].is<JsonArray>() || document["services"].size() == 0U)
  {
    return;
  }
  service = document["services"][0].as<JsonObject>();
  serviceId = service["service_id"] | "";
  eventType = service["event_type"] | "";
  paras = service["paras"].as<JsonObject>();
  if (String(serviceId) != "$ota")
  {
    return;
  }
  if (String(eventType) == "version_query")
  {
    reportCurrentVersion();
    return;
  }
  bool isPlatformPackage = String(eventType) == "firmware_upgrade";
  bool isObsPackage = String(eventType) == "firmware_upgrade_v2";
  if (!isPlatformPackage && !isObsPackage)
  {
    return;
  }

  const char *version = paras["version"] | "";
  const char *url = paras["url"] | "";
  const char *sign = paras["sign"] | "";
  const char *customInfo = paras["custom_info"] | "";
  const char *token = paras["access_token"] | "";
  uint32_t fileSize = paras["file_size"] | 0U;
  char slot = String(customInfo).indexOf("slot=A") >= 0 ? 'A' :
              (String(customInfo).indexOf("slot=B") >= 0 ? 'B' : '\0');

  /* STM32 会用 SHA-256 校验整包，故即使 OBS 事件中 sign 是可选项，这个工程也要求提供。 */
  if ((String(version).length() == 0U) || (String(url).length() < 8U) ||
      (String(sign).length() != 64U) || ((fileSize == 0U) || ((fileSize & 1U) != 0U)) ||
      (slot == '\0'))
  {
    return;
  }

  /* 这里只保存任务；HTTP 下载由主循环 processPendingOta() 顺序执行。 */
  pendingPackage_.version = version;
  pendingPackage_.targetSlot = slot;
  pendingPackage_.fileSize = fileSize;
  pendingPackage_.sha256 = sign;
  pendingPackage_.sha256.toLowerCase();
  pendingPackage_.url = url;
  pendingPackage_.accessToken = token;
  /* 华为云官方协议：平台存储包可带 Bearer token；OBS v2 下载地址本身已签名，不加该头。 */
  pendingPackage_.needsAuthorization = isPlatformPackage && (String(token).length() > 0U);
  packagePending_ = true;
}
