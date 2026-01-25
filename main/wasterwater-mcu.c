// #include "adc/adc.h"
// #include "uart/uart.h"
// #include "freertos/FreeRTOS.h"
// #include "esp_log.h"
// #include "driver/uart.h"
// #include "driver/gpio.h"
// #include "gpio/pwm.h"
//
// static const char* TAG = "MAIN";
//
// // ============================
// // UART 接收回调
// // ============================
// void uart_receive_callback(const uint8_t* data, size_t length)
// {
//     ESP_LOGI(TAG, "UART received %u bytes", length);
//     uart_send_data(UART_NUM_0, data, length);
// }
//
// // ============================
// // ADC 回调（新版！！！）
// // ============================
// // ★ adc_tool_start 会在后台任务中不断调用这个函数
// void adc_value_callback(int raw, int voltage_mv)
// {
//     char buffer[64];
//     int len = snprintf(buffer, sizeof(buffer),
//                        "ADC RAW: %d, Voltage: %d mV\r\n",
//                        raw, voltage_mv);
//
//     uart_send_data(UART_NUM_0, (const uint8_t*)buffer, len);
//     ESP_LOGI("ADC_CALLBACK", "RAW=%d, V=%d mV", raw, voltage_mv);
// }
//
// // ============================
// // PWM控制任务
// // ============================
// #define PWM_GPIO_NUM           7
// #define PWM_FREQUENCY          20000  // 20kHz
// #define PWM_DUTY_RESOLUTION    LEDC_TIMER_10_BIT  // 10位分辨率 (0-1023)
// #define PWM_MAX_DUTY           1023   // 最大占空比值 (2^10 - 1)
// #define PWM_MIN_DUTY           700    // 20%占空比 (1023 * 0.2)
// #define PWM_MID_DUTY           800    // 80%占空比 (1023 * 0.8)
// #define PWM_CYCLE_PERIOD_MS    1000   // 1秒周期
//
// static pwm_config_t pwm_config = {
//     .timer_num = LEDC_TIMER_0,
//     .speed_mode = LEDC_LOW_SPEED_MODE,
//     .frequency = PWM_FREQUENCY,
//     .duty_resolution = PWM_DUTY_RESOLUTION,
//     .channel = LEDC_CHANNEL_0,
//     .gpio_num = PWM_GPIO_NUM,
//     .duty = PWM_MIN_DUTY,  // 初始占空比为20%
// };
//
// /**
//  * @brief PWM控制任务
//  *
//  * 控制PWM占空比在20%-80%-20%之间循环变化，周期为1秒
//  */
// void pwm_control_task(void)
// {
//     static uint8_t state = 0; // 0: 20%, 1: 80%, 2: 20%
//
//     switch (state) {
//         case 0: // 保持20%占空比500ms
//             ESP_LOGD("PWM_TASK", "Setting PWM duty to 20%%");
//             pwm_set_duty(pwm_config.channel, PWM_MIN_DUTY, pwm_config.speed_mode);
//             state = 1;
//             break;
//
//         case 1: // 改变到80%占空比并保持500ms
//             ESP_LOGD("PWM_TASK", "Setting PWM duty to 80%%");
//             pwm_set_duty(pwm_config.channel, PWM_MID_DUTY, pwm_config.speed_mode);
//             state = 2;
//             break;
//
//         case 2: // 改变回20%占空比
//             ESP_LOGD("PWM_TASK", "Setting PWM duty back to 20%%");
//             pwm_set_duty(pwm_config.channel, PWM_MIN_DUTY, pwm_config.speed_mode);
//             state = 0;
//             break;
//
//         default:
//             state = 0;
//             break;
//     }
// }
//
// // ============================
// // app_main
// // ============================
// void app_main(void)
// {
//     ESP_LOGI(TAG, "System booting...");
//
//     // 初始化 UART0
//     esp_err_t ret = uart_init(UART_NUM_0, GPIO_NUM_9, GPIO_NUM_8, 115200, uart_receive_callback);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to initialize UART0");
//     }
//     else
//     {
//         const char* msg = "UART0 initialized successfully\r\n";
//         uart_send_data(UART_NUM_0, (const uint8_t*)msg, strlen(msg));
//         ESP_LOGI(TAG, "UART0 init OK");
//     }
//
//     // ============================
//     // 启动 ADC（★ 新的函数）
//     // ============================
//     ret = adc_tool_start(
//             ADC_UNIT_1,
//             ADC_CHANNEL_3,
//             ADC_ATTEN_DB_12,
//             adc_value_callback   // ★ 用户回调函数
//     );
//
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to start ADC tool: %s", esp_err_to_name(ret));
//         return;
//     }
//
//     ESP_LOGI(TAG, "ADC tool started successfully");
//
//     // ============================
//     // 初始化 PWM 输出
//     // ============================
//     ret = pwm_init(&pwm_config);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to initialize PWM: %s", esp_err_to_name(ret));
//     } else {
//         ESP_LOGI(TAG, "PWM initialized successfully on GPIO%d with %d Hz frequency",
//                  PWM_GPIO_NUM, PWM_FREQUENCY);
//     }
//
//     // ============================
//     // 主循环 - 控制 PWM 占空比变化
//     // ============================
//     // 占空比变化周期: 20% -> 80% -> 20%，每1秒切换一次状态
//     static uint32_t last_pwm_update = 0;
//     while (1) {
//         uint32_t current_time = xTaskGetTickCount();
//
//         // 每隔一定时间更新PWM占空比
//         if ((current_time - last_pwm_update) >= pdMS_TO_TICKS(500)) {
//             pwm_control_task();
//             last_pwm_update = current_time;
//         }
//
//         vTaskDelay(pdMS_TO_TICKS(10)); // 小延时以避免过度占用CPU
//     }
// }
//
// //测试
//
// #include <stdio.h>
// #include <string.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "driver/uart.h"
// #include "driver/gpio.h"
// #include "esp_log.h"
//
// // --- 硬件定义 ---
// #define UART_PORT_NUM      UART_NUM_0
// #define UART_BAUD_RATE     115200
// #define TXD_PIN            (GPIO_NUM_1)
// #define RXD_PIN            (GPIO_NUM_3)
// #define BUF_SIZE           (1024)
//
// // --- 数据结构 ---
// typedef struct {
//     char name[16];
//     char trigger_key;
//     float current_val;
//     float base_val;
//     float target_val;
//     float step;
//     bool is_active;
// } sensor_attr_t;
//
// typedef struct {
//     char name[16];
//     char trigger_key;
//     gpio_num_t gpio_pin;
//     bool state;
// } status_attr_t;
//
// // --- 可配置数据 ---
// sensor_attr_t sensors[] = {
//     {"Temp", '1', 25.0f, 25.0f, 42.5f, 0.3f, false},
//     {"Humi", '2', 45.0f, 45.0f, 80.0f, 1.2f, false},
//     {"Lux",  '3', 300.0f, 300.0f, 1200.0f, 25.0f, false}
// };
//
// status_attr_t status_devs[] = {
//     {"Relay", '5', GPIO_NUM_2, false} // ESP32 DevKit 常用 GPIO2 作为板载 LED
// };
//
// #define SENSOR_COUNT (sizeof(sensors)/sizeof(sensor_attr_t))
// #define STATUS_COUNT (sizeof(status_devs)/sizeof(status_attr_t))
//
// // --- 硬件初始化 ---
// void init_hw(void) {
//     const uart_config_t uart_config = {
//         .baud_rate = UART_BAUD_RATE,
//         .data_bits = UART_DATA_8_BITS,
//         .parity    = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_DEFAULT,
//     };
//     uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
//     uart_param_config(UART_PORT_NUM, &uart_config);
//
//     for(int i=0; i<STATUS_COUNT; i++) {
//         gpio_reset_pin(status_devs[i].gpio_pin);
//         gpio_set_direction(status_devs[i].gpio_pin, GPIO_MODE_OUTPUT);
//         gpio_set_level(status_devs[i].gpio_pin, status_devs[i].state);
//     }
// }
//
// // --- 任务1: 模拟变化与静默输出 (运行在 Core 0) ---
// void monitor_task(void *pvParameters) {
//     while(1) {
//         // 1. 步进逻辑计算
//         for(int i=0; i<SENSOR_COUNT; i++) {
//             float goal = sensors[i].is_active ? sensors[i].target_val : sensors[i].base_val;
//             if (sensors[i].current_val < goal) {
//                 sensors[i].current_val += sensors[i].step;
//                 if (sensors[i].current_val > goal) sensors[i].current_val = goal;
//             } else if (sensors[i].current_val > goal) {
//                 sensors[i].current_val -= sensors[i].step;
//                 if (sensors[i].current_val < goal) sensors[i].current_val = goal;
//             }
//         }
//
//         // 2. 串口刷新输出
//         printf(">> ");
//         for(int i=0; i<SENSOR_COUNT; i++) {
//             printf("%s: %.2f | ", sensors[i].name, sensors[i].current_val);
//         }
//         for(int i=0; i<STATUS_COUNT; i++) {
//             printf("%s: %s ", status_devs[i].name, status_devs[i].state ? "ON " : "OFF");
//         }
//         printf("\n");
//
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }
//
// // --- 任务2: 静默 UART 监听 (运行在 Core 1) ---
// void uart_rx_task(void *pvParameters) {
//     uint8_t data;
//     while (1) {
//         // 阻塞读取
//         int len = uart_read_bytes(UART_PORT_NUM, &data, 1, portMAX_DELAY);
//         if (len > 0) {
//             // 处理传感器切换
//             for(int i=0; i<SENSOR_COUNT; i++) {
//                 if(data == sensors[i].trigger_key) {
//                     sensors[i].is_active = !sensors[i].is_active;
//                 }
//             }
//             // 处理状态切换
//             for(int i=0; i<STATUS_COUNT; i++) {
//                 if(data == status_devs[i].trigger_key) {
//                     status_devs[i].state = !status_devs[i].state;
//                     gpio_set_level(status_devs[i].gpio_pin, status_devs[i].state);
//                 }
//             }
//         }
//     }
// }
//
// void app_main(void) {
//     init_hw();
//
//     // 在不同核心上创建任务
//     xTaskCreatePinnedToCore(monitor_task, "monitor", 4096, NULL, 5, NULL, 0);
//     xTaskCreatePinnedToCore(uart_rx_task, "uart_rx", 4096, NULL, 10, NULL, 1);
// }


#include "rtu.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

void app_main(void)
{
    xTaskCreate(modbus_task, "modbus_task", 4096, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(10));
}
