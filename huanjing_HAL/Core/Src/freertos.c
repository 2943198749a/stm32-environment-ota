/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-07-29 17:33:44
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-09-17 16:26:58
 * @FilePath: \MDK-ARMd:\aiguigu\huanjing_freeRTOS_HAL\huanjing_freeRTOS_HAL\huanjing_HAL\Core\Src\freertos.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-07-29 17:33:44
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-09-17 16:26:54
 * @FilePath: \MDK-ARMd:\aiguigu\huanjing_freeRTOS_HAL\huanjing_freeRTOS_HAL\huanjing_HAL\Core\Src\freertos.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : 初学者版 FreeRTOS 任务示例。
  *
  * 手写代码的数据流：
  *   传感器任务 -> latestEnvData -> UI 任务和普通串口任务
  *   USART2 中断 -> 环形缓冲区 -> OTA 任务 -> OTA 协议/ESP32 状态
  *
  * 四个任务的职责：
  *   StartDefaultTask：每秒读取 DHT11、BH1750 和 MQ135；
  *   StartTask02：读取按键、切换页面并刷新 LCD；
  *   StartTask03：每秒把完整环境快照发给 ESP32；
  *   StartOtaTask：持续解析串口协议，并在新固件稳定后确认启动。
  *
  * CubeMX 会管理任务句柄和创建框架，自写内容必须保留在 USER CODE 区域。
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "env_sensor.h"
#include "app_key.h"
#include "app_ui.h"
#include "esp32_uart.h"
#include "esp32_status.h"

#include <string.h>
#include <stdio.h>
#include "ota_app.h"
#include "ota_uart.h"
#include "ota_port.h"
#include "ota_port_config.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/*
 * 传感器任务写入这份最新环境数据，LCD 任务读取它。
 * 不使用复杂的互斥锁：结构体复制只需要很短时间，因此使用临界区即可。
 */
static EnvData_t latestEnvData;
/* USER CODE END Variables */
/* Definitions for sensortask */
osThreadId_t sensortaskHandle;
const osThreadAttr_t sensortask_attributes = {
  .name = "sensortask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for uitask */
osThreadId_t uitaskHandle;
const osThreadAttr_t uitask_attributes = {
  .name = "uitask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for uarttask */
osThreadId_t uarttaskHandle;
const osThreadAttr_t uarttask_attributes = {
  .name = "uarttask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for otaTask */
osThreadId_t otaTaskHandle;
const osThreadAttr_t otaTask_attributes = {
  .name = "otaTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal1,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void RtosData_SaveEnvironment(void);
static void RtosData_CopyEnvironment(EnvData_t *environment);
static bool RtosData_IsEnvironmentReady(const EnvData_t *environment);
static void RtosData_SendEnvironment(const EnvData_t *environment);
static bool RtosTasks_AreCreated(void);
static bool RtosOta_ConfirmWhenStable(uint32_t boot_time);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTask02(void *argument);
void StartTask03(void *argument);
void StartOtaTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* 这里只清空软件数据；传感器初始化必须等传感器任务运行后再做。 */
  /* 先初始化跨任务模块，再创建任务；任务一旦运行就可以直接使用它们。 */
  (void)memset(&latestEnvData, 0, sizeof(latestEnvData));
  Esp32Status_Init();
	Esp32Uart_Init();
	OtaApp_Init();
	OtaUart_Init(OTA_APPLICATION_SLOT);
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* 此初学者版本不需要创建互斥锁。 */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of sensortask */
  sensortaskHandle = osThreadNew(StartDefaultTask, NULL, &sensortask_attributes);

  /* creation of uitask */
  uitaskHandle = osThreadNew(StartTask02, NULL, &uitask_attributes);

  /* creation of uarttask */
  uarttaskHandle = osThreadNew(StartTask03, NULL, &uarttask_attributes);

  /* creation of otaTask */
  otaTaskHandle = osThreadNew(StartOtaTask, NULL, &otaTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /*
   * 动态创建任务会从 FreeRTOS 堆申请内存；返回 NULL 说明堆不够。
   * 此时立即进入 Error_Handler，避免系统看似启动但实际少了关键任务。
   */
  if (!RtosTasks_AreCreated())
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  任务一：采集温湿度、光照和空气质量。
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  (void)argument;

  /*
   * 传感器没有接线时，I2C 读取可能等待超时。
   * 把初始化放进任务内，能保证 FreeRTOS 调度器和 LCD 不会在启动前被卡住。
   */
  EnvSensor_Init();

  for (;;)
  {
    EnvSensor_Update();
    RtosData_SaveEnvironment();

    /* 每秒采样一次；osDelay 会主动让出 CPU，不能改成 HAL_Delay。 */
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
  * @brief  任务二：处理 KEY1 并刷新 LCD。
  */
/* USER CODE END Header_StartTask02 */
void StartTask02(void *argument)
{
  /* USER CODE BEGIN StartTask02 */
  EnvData_t environment;
  Esp32Status_t esp32_status;
  (void)argument;

  /* LCD 硬件已在 main.c 初始化，这里只初始化按键与页面状态。 */
  AppKey_Init();
  AppUi_Init();

  for (;;)
  {
    if (AppKey_PageTogglePressed())
    {
      AppUi_TogglePage();
    }

    /* 先复制到局部变量，再绘制 LCD；绘制过程不占用临界区。 */
    RtosData_CopyEnvironment(&environment);
    esp32_status = Esp32Status_Get();
    AppUi_Render(&environment, &esp32_status);

    /* 每 500 ms 刷新一次，人眼看起来稳定，也不会频繁操作 LCD。 */
    osDelay(500);
  }
  /* USER CODE END StartTask02 */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
  * @brief  任务三：与 Arduino ESP32 进行普通 UART 通信。
  */
/* USER CODE END Header_StartTask03 */
void StartTask03(void *argument)
{
  /* USER CODE BEGIN StartTask03 */

  EnvData_t environment;

  (void)argument;

  for (;;)
  {
    /*
     * 从任务一保存的共享数据中复制一份。
     * 不直接读取传感器，避免与任务一同时访问传感器模块。
     */
    RtosData_CopyEnvironment(&environment);

    /*
     * 四项数据有效，并且当前不在 OTA 升级中，才发送。
     * OTA 期间禁止发送 ENV，避免干扰 OTA 数据帧和应答。
     */
    if (RtosData_IsEnvironmentReady(&environment) && !OtaUart_IsBusy())
    {
      RtosData_SendEnvironment(&environment);
    }

    /* 每秒上传一次。 */
    osDelay(1000);
  }

  /* USER CODE END StartTask03 */
}
/* USER CODE BEGIN Header_StartOtaTask */
/**
* @brief 任务四：消费 USART2 字节、推进 OTA 状态机并确认试运行的新固件。
* @param argument CubeMX 任务参数，本项目未使用。
*
* OTA 协议可能擦写 Flash，因此必须在任务上下文运行，不能放进 UART 中断。
* 循环周期为 1 ms，可及时取走环形缓冲区数据；osDelay 会让其他任务继续运行。
*/
/* USER CODE END Header_StartOtaTask */
void StartOtaTask(void *argument)
{
  /* USER CODE BEGIN StartOtaTask */

  uint32_t boot_time;
  bool confirmed = false;

  (void)argument;

  /* 记录本次程序启动时间。 */
  /* 十秒确认窗口从 OTA 任务真正开始运行时计时。 */
  boot_time = HAL_GetTick();

  for (;;)
  {
    /*
     * 处理 ESP32 通过 USART2 发来的数据：
     * STATUS、OTA_BEGIN、固件二进制帧、OTA_END 等。
     */
    /* 一次调用会消费当前已缓存字节，并可能推进文本或二进制帧状态机。 */
    OtaUart_Process();

    /*
     * 如果当前运行的是刚升级的新分区，
     * 并且稳定运行了 10 秒，就确认升级成功。
     */
    if (!confirmed && RtosOta_ConfirmWhenStable(boot_time))
    {
      confirmed = true;
    }

    /*
     * OTA 必须频繁执行，避免 UART 数据堆积。
     * 延时 1 ms 后其他任务仍可运行。
     */
    osDelay(1);
  }

  /* USER CODE END StartOtaTask */
}
/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* 将传感器模块内部的最新一帧数据复制到任务共享副本。 */
static void RtosData_SaveEnvironment(void)
{
  const EnvData_t *environment = EnvSensor_GetData();

  if (environment == NULL)
  {
    return;
  }

  /* 临界区只保护一条结构体赋值，时间极短，不会影响其他任务正常调度。 */
  taskENTER_CRITICAL();
  latestEnvData = *environment;
  taskEXIT_CRITICAL();
}

/* 将共享副本复制给 LCD 任务，避免它读到传感器正在更新的一半数据。 */
static void RtosData_CopyEnvironment(EnvData_t *environment)
{
  if (environment == NULL)
  {
    return;
  }

  taskENTER_CRITICAL();
  *environment = latestEnvData;
  taskEXIT_CRITICAL();
}

/* 四项数据必须来自同一份受保护快照；任一传感器失败时不上传半帧数据。 */
static bool RtosData_IsEnvironmentReady(const EnvData_t *environment)
{
  return (environment != NULL) &&
         environment->temperature_valid &&
         environment->humidity_valid &&
         environment->light_valid &&
         environment->air_valid;
}

/*
 * 把定点数环境数据转换成 ESP32 约定的 ENV 文本。
 * 格式化和串口发送可能耗时，因此调用者必须先把共享数据复制到局部变量，
 * 不能在 FreeRTOS 临界区内调用本函数。
 */
static void RtosData_SendEnvironment(const EnvData_t *environment)
{
  char send_text[64];
  int length;
  int32_t temperature_abs;

  if (environment == NULL)
  {
    return;
  }

  temperature_abs = (int32_t)environment->temperature_tenth;
  if (temperature_abs < 0)
  {
    temperature_abs = -temperature_abs;
  }

  length = snprintf(send_text, sizeof(send_text),
                    "ENV,%s%ld.%ld,%u.%u,%u,%u\r\n",
                    (environment->temperature_tenth < 0) ? "-" : "",
                    (long)(temperature_abs / 10),
                    (long)(temperature_abs % 10),
                    (unsigned int)(environment->humidity_tenth / 10U),
                    (unsigned int)(environment->humidity_tenth % 10U),
                    (unsigned int)environment->light_lux,
                    (unsigned int)environment->air_ppm);

  if ((length > 0) && ((uint32_t)length < sizeof(send_text)))
  {
    (void)Esp32Uart_SendString(send_text);
  }
}

/* 动态任务必须全部创建成功；少一个任务都会留下表面运行、功能残缺的系统。 */
static bool RtosTasks_AreCreated(void)
{
  return (sensortaskHandle != NULL) &&
         (uitaskHandle != NULL) &&
         (uarttaskHandle != NULL) &&
         (otaTaskHandle != NULL);
}

/*
 * 新镜像稳定运行 10 秒后才确认，给启动故障和早期崩溃留下回滚窗口。
 * 返回 true 只表示元数据确认成功；调用者据此停止重复确认。
 */
static bool RtosOta_ConfirmWhenStable(uint32_t boot_time)
{
  char text[40];
  int length;

  /* 第一步：普通开机不用确认，只有刚升级、等待确认的新程序才需要。 */
  if (!OtaApp_IsConfirmationRequired(OTA_APPLICATION_SLOT))
  {
    return false;
  }

  /* 第二步：从 OTA 任务开始运行计时，未满 10 秒就继续等待。
   * 这里只判断经过的时间，不代表已经检查了所有传感器和屏幕功能。 */
  if ((HAL_GetTick() - boot_time) < 10000U)
  {
    return false;
  }

  /* 第三步：把当前区记为已确认区，保存失败时留到下一轮重试。 */
  if (OtaApp_ConfirmRunning(OTA_APPLICATION_SLOT) != HAL_OK)
  {
    return false;
  }

  /* 第四步：通知 ESP32 新程序已确认，发送内容保持原协议。 */

  length = snprintf(text, sizeof(text), "BOOT_OK,%s\r\n", OTA_CURRENT_VERSION);
  if ((length > 0) && ((uint32_t)length < sizeof(text)))
  {
    OtaPort_Send((const uint8_t *)text, (uint16_t)length);
  }

  return true;
}

/* USER CODE END Application */

