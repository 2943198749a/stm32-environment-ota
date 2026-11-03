/*
 * ESP32-C3 的 USART2 字节传输实现。
 *
 * HAL 接收中断是环形缓冲区的唯一写入者，OTA 任务是唯一读取者。
 * 中断只保存字节，不在中断中解析协议、操作 LCD、写 Flash 或打印日志。
 *
 * 把环形缓冲区理解为“一个数组 + 两个下标”：
 *   g_rx_write：下一个收到的字节应该放在哪里。
 *   g_rx_read ：下一个待处理的字节应该从哪里取。
 * 两个下标走到 255 后都回到 0，数组本身不需要移动。
 * read == write 表示空；写位置再走一步就碰到 read 表示满。
 * 为区分空和满，必须留一个空位，256 个位置实际最多存 255 字节。
 * 满时丢弃新字节，保留未读的旧数据；有限的缓冲区不能保证永不丢包。
 *
 * 例：初始化 read=0、write=0，表示空。
 * 收到 0x41：buffer[0]=0x41，write=1；此时有一个字节可读。
 * 任务取走 buffer[0] 后 read=1；两个位置又相同，表示已读完。
 */
/* 声明本模块的函数，并提供 bool、uint8_t、uint16_t 等类型。 */
#include "esp32_uart.h"
/* 提供 CubeMX 创建的 huart2 串口句柄和相关 HAL 声明。 */
#include "usart.h"

/* 提供 strlen，用来计算待发送字符串的长度。 */
#include <string.h>

/* #define 定义常量；100U 中 U 表示无符号整数。发送超时参数为 100 ms。 */
#define ESP32_UART_SEND_TIMEOUT_MS  100U
/* 数组长度为 256，合法下标为 0~255；任务及时取走数据才能避免积满。 */
#define ESP32_UART_RX_BUFFER_SIZE  256U

/* static：变量只在本文件可见，函数退出后仍保留。
 * uint8_t：8 位无符号整数，存一个字节；HAL 将新收到的字节写入这里。 */
static uint8_t g_rx_byte;
/* volatile：中断和任务共享的数据，每次访问都必须按代码要求实际读写。
 * 它不是锁，也不能保证任意多步操作不可被中断；本实现只有一个写入者和一个读取者。 */
static volatile uint8_t g_rx_buffer[ESP32_UART_RX_BUFFER_SIZE]; /* 环形数据区。 */
/* uint16_t：16 位无符号整数；写下标只由接收中断推进。 */
static volatile uint16_t g_rx_write; /* ISR 下一次写入位置。 */
/* 读下标只由 OTA 任务推进；中断只观察它来判断是否有空位。 */
static volatile uint16_t g_rx_read;  /* OTA 任务下一次读取位置。 */


/* void 返回类型表示没有返回值；括号内 void 表示不需要参数。
 * 在开机、串口硬件初始化后调用，不要在接收过程中反复清空下标。 */
void Esp32Uart_Init(void)
{
    /*
     * huart2 已由 CubeMX 的 MX_USART2_UART_Init() 初始化。
     * 这里不重复初始化硬件，只将软件缓存标记为空，再启动接收。
     * 不必把数组内容全部清零：两个下标相同就表示没有有效的未读数据。
     */
    /* 下一次接收从数组第 0 格开始保存。 */
    g_rx_write = 0U;
    /* 下一次读取也从第 0 格开始；此时 read == write，缓存为空。 */
    g_rx_read = 0U;
    /* Receive_IT 启动中断接收，不在这里等待数据到达。
     * &huart2：串口句柄的地址，告诉 HAL 使用 USART2。
     * &g_rx_byte：接收目的地址；1U：这次接收一个字节。
     * (void) 表示忽略返回状态，函数仍会执行；当前代码不处理启动失败。 */
    (void)HAL_UART_Receive_IT(&huart2, &g_rx_byte, 1U);
}

/* 发送字符串。text 指向以 '\0' 结尾的有效字符串；const 表示不通过它修改字符。
 * 返回 bool：true 表示 HAL 报告发送成功，false 表示参数无效或发送失败。 */
bool Esp32Uart_SendString(const char *text)
{
    /* 局部变量，只在这次调用中使用；保存要发送的字节数。 */
    uint16_t text_length;

    /* NULL 是空指针；没有有效的字符串地址，就不能调用 strlen 读取内容。 */
    if (text == NULL)
    {
        /* return 立即结束当前函数，并把 false 交给调用者。 */
        return false;
    }

    /* strlen 计算 '\0' 前的字节数，不包含结尾的 '\0'。
     * (uint16_t) 将长度转成 HAL 接口使用的类型；本接口用于短字符串，
     * 超过 65535 字节的长度会在转换时截断，不能用它发送超长字符串。 */
    text_length = (uint16_t)strlen(text);

    /* 这是阻塞发送，调用者等待发送完成或出错/超时；不是中断发送。
     * 四个参数依次是：串口句柄地址、数据地址、字节数、超时毫秒数。
     * (uint8_t *) 适配 HAL 的指针参数类型；这里发送原字符串，不复制内容。
     * == HAL_OK 将 HAL 状态转换成 bool：成功返回 true，否则返回 false。
     * HAL_OK 的数值是 0，所以不能直接把 HAL 返回值当作 bool 返回。 */
    return HAL_UART_Transmit(&huart2, (uint8_t *)text, text_length,
                             ESP32_UART_SEND_TIMEOUT_MS) == HAL_OK;
}

/*
 * 当前配置每次接收一个字节，接收完成后 HAL 在中断处理中调用这里。
 * UART_HandleTypeDef 是 HAL 的结构体类型；huart 指向本次触发回调的串口句柄。
 * 注意：不要在中断里画 LCD、printf 或做复杂处理，只存数据。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint16_t next_write; /* 临时计算下一格；检查有空间以后才更新真正的写下标。 */

    /* -> 通过结构体指针访问成员；Instance 表示具体的串口外设。
     * != 表示不等于：其他串口的回调不在本模块处理。 */
    if (huart->Instance != USART2)
    {
        /* 此函数不返回数值，所以只写 return；后面的接收处理不再执行。 */
        return;
    }

    /* +1 表示向后走一格；% 是取余数，用来实现走到末尾后回到开头。
     * write=0 时：(0+1)%256=1；write=255 时：(255+1)%256=0。
     * (uint16_t) 将计算结果转换成下标使用的类型。 */
    next_write = (uint16_t)((g_rx_write + 1U) % ESP32_UART_RX_BUFFER_SIZE);

    /* 下一格不等于读下标，说明还能存；若相等则不执行大括号内的代码。
     * 满时不能推进写下标，否则会把未读数据覆盖，或把“满”误表示为“空”。 */
    if (next_write != g_rx_read)
    {
        g_rx_buffer[g_rx_write] = g_rx_byte; /* 先将新字节存入当前空位。 */
        g_rx_write = next_write; /* 再公布新的写位置，让任务知道有数据可取。 */
    }

    /* 无论本字节是否因缓存满而被丢弃，都要再次启动下一字节的中断接收。
     * HAL 的本次单字节接收已完成，不重新启动就不会继续这条接收流程。
     * 当前实现忽略返回状态，没有在这里做接收启动失败的恢复处理。 */
    (void)HAL_UART_Receive_IT(&huart2, &g_rx_byte, 1U);
}
/* OTA 任务调用：有数据就取一个字节，没有数据立即返回，不等待。
 * byte 是调用者提供的存储地址。例如 uint8_t value; Esp32Uart_ReadByte(&value);
 * 成功后 value 得到读出的字节；只能由一个任务调用此函数消费数据。 */
bool Esp32Uart_ReadByte(uint8_t *byte)
{
    /* || 是逻辑“或”，任一条件成立就返回 false：
     * byte == NULL：没有可写的输出地址。
     * read == write：没有尚未读取的数据。此时不修改调用者的变量。 */
    if ((byte == NULL) || (g_rx_read == g_rx_write))
    {
        return false; /* 本次没有取到字节。 */
    }

    /* *byte 是解引用：将数组中的字节写到调用者提供的地址。
     * 例如传入 &value，这一句就会修改调用者的 value。
     * 必须先读取数据再推进读下标，避免中断过早复用这个位置。
     * 此处依赖单中断写入、单任务读取，以及 Cortex-M3 的对齐 16 位访问。 */
    *byte = g_rx_buffer[g_rx_read];
    /* 读下标向后走一格；255 的下一格回到 0，与写下标使用相同的回绕规则。
     * 原数组字节不必擦掉：移动 read 就表示该位置的数据已经消费，可以复用。 */
    g_rx_read = (uint16_t)((g_rx_read + 1U) % ESP32_UART_RX_BUFFER_SIZE);

    return true; /* 已成功取出一个字节，调用者现在可以处理它。 */
}
