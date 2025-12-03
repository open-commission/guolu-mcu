//
// Created by nebula on 2025/12/3.
//

#include "uart.h"

/* UART asynchronous example, that uses separate RX and TX tasks
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// 包含 FreeRTOS 的基础头文件（任务、延时等 API）
#include "freertos/FreeRTOS.h"    // FreeRTOS 基本类型与宏
#include "freertos/task.h"        // xTaskCreate / vTaskDelay 等任务 API

// ESP 平台基础头文件（系统、日志）
#include "esp_log.h"              // ESP_LOGx 系列日志宏

// UART 驱动头文件
#include "driver/uart.h"          // uart 驱动 API（uart_driver_install、uart_read_bytes 等）

// 字符串处理
#include "string.h"               // strlen 等字符串函数

// GPIO 定义（若需控制引脚，例如 rs485 的 DE）
#include "driver/gpio.h"          // gpio 控制（示例中未直接使用，但通常会需要）

// 定义接收缓冲区大小（字节）
static const int RX_BUF_SIZE = 1024; // 用于 uart_read_bytes 的缓冲区大小

// TXD_PIN 和 RXD_PIN 使用 sdkconfig 中的配置宏（在 menuconfig 中设置）
#define TXD_PIN (CONFIG_EXAMPLE_UART_TXD) // 串口 TX 引脚，从 sdkconfig 获取
#define RXD_PIN (CONFIG_EXAMPLE_UART_RXD) // 串口 RX 引脚，从 sdkconfig 获取

// 初始化 UART 的函数（配置波特率、数据位、校验位、停止位、流控等）
void init(void)
{
    // 定义 uart 的配置结构体并初始化字段
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_EXAMPLE_UART_BAUD_RATE, // 波特率，从 sdkconfig 获取
        .data_bits = UART_DATA_8_BITS,              // 数据位：8 位
        .parity = UART_PARITY_DISABLE,              // 无校验
        .stop_bits = UART_STOP_BITS_1,              // 停止位：1
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      // 禁用硬件流控（RTS/CTS）
        .source_clk = UART_SCLK_DEFAULT,            // 时钟源，使用默认
    };

    // 安装 UART 驱动：
    // 参数：UART_NUM_1 - 使用 UART1
    //       RX_BUF_SIZE * 2 - 驱动层的接收环形缓冲区大小（此处设置为两倍 RX_BUF_SIZE）
    //       0 - 发送缓冲区大小（设置为 0 表示不使用驱动层发送缓冲）
    //       0 - 事件队列长度（0 表示不使用事件队列）
    //       NULL - 事件队列句柄（不使用则为 NULL）
    //       0 - 中断分配标志
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);

    // 应用上面定义的串口参数（把 uart_config 应用到 UART_NUM_1）
    uart_param_config(UART_NUM_1, &uart_config);

    // 设置 UART 引脚（TX, RX, RTS, CTS）
    // UART_PIN_NO_CHANGE 表示不改变对应引脚（此示例没有使用 RTS/CTS）
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// 发送数据的封装函数：返回实际写入的字节数
int sendData(const char* logName, const char* data)
{
    const int len = strlen(data); // 计算字符串长度（不包含末尾 '\0'）
    // uart_write_bytes 会把 data 写入驱动层或直接发送，返回写入的字节数
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    // 打印一条 info 级别日志，输出写入的字节数
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes; // 返回写入长度给调用者
}

// TX 任务：周期性发送字符串
static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";                 // 日志标签
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);               // 设置该标签的日志级别为 INFO

    while (1) {
        // 每次循环调用 sendData 发送 "Hello world"
        sendData(TX_TASK_TAG, "Hello world");
        // 任务休眠 2000 ms（2 秒），portTICK_PERIOD_MS 是系统滴答周期换算宏
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    // 注意：此处没有任务退出或 free 的逻辑，因为该任务是长期运行的
}

// RX 任务：循环读取串口数据并打印
static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";                 // 日志标签
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);               // 设置该标签的日志级别为 INFO

    // 为接收分配缓冲区（+1 用来放置 C 字符串终止符 '\0'）
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);

    while (1) {
        // 从 UART_NUM_1 读取最多 RX_BUF_SIZE 字节，超时为 1000ms（以系统滴答为单位）
        // uart_read_bytes 返回实际读取的字节数，若超时或无数据则返回 0
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);

        if (rxBytes > 0) {                       // 如果读取到数据
            data[rxBytes] = 0;                   // 追加字符串终止符，便于按字符串打印（注意：接收的数据可能包含 '\0'）
            // 打印以可读字符串形式显示读取到的数据及字节数
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            // 打印十六进制转储，便于调试二进制数据
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
        // 循环继续再次等待新数据（此处没有延时，uart_read_bytes 内含阻塞/超时）
    }

    // 如果代码能执行到这里（实际上不会，因为上面是无限循环），释放分配的内存
    free(data);
}

// 应用入口函数（ESP-IDF 中的 app_main）
void app_main(void)
{
    init(); // 初始化 UART 驱动与参数

    // 创建 RX 任务：
    // 参数依次为：任务函数，任务名称，栈大小，参数，优先级，任务句柄（这里不需要句柄）
    // CONFIG_EXAMPLE_TASK_STACK_SIZE：在 sdkconfig 中配置的任务栈大小
    // 优先级使用 configMAX_PRIORITIES - 1（较高优先级，保留更高值给系统/必要任务）
    xTaskCreate(rx_task, "uart_rx_task", CONFIG_EXAMPLE_TASK_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);

    // 创建 TX 任务：优先级略低于 RX 任务（configMAX_PRIORITIES - 2）
    xTaskCreate(tx_task, "uart_tx_task", CONFIG_EXAMPLE_TASK_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
}