#include "adc/adc.h"
#include "uart/uart.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
static const char* TAG = "MAIN";

// ============================
// UART 接收回调
// ============================
void uart_receive_callback(const uint8_t* data, size_t length)
{
    ESP_LOGI(TAG, "UART received %u bytes", length);
    uart_send_data(UART_NUM_0, data, length);
}

// ============================
// ADC 回调（新版！！！）
// ============================
// ★ adc_tool_start 会在后台任务中不断调用这个函数
void adc_value_callback(int raw, int voltage_mv)
{
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer),
                       "ADC RAW: %d, Voltage: %d mV\r\n",
                       raw, voltage_mv);

    uart_send_data(UART_NUM_0, (const uint8_t*)buffer, len);
    ESP_LOGI("ADC_CALLBACK", "RAW=%d, V=%d mV", raw, voltage_mv);
}

// ============================
// app_main
// ============================
void app_main(void)
{
    ESP_LOGI(TAG, "System booting...");

    // 初始化 UART0
    esp_err_t ret = uart_init(UART_NUM_0, GPIO_NUM_9, GPIO_NUM_8, 115200, uart_receive_callback);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize UART0");
    }
    else
    {
        const char* msg = "UART0 initialized successfully\r\n";
        uart_send_data(UART_NUM_0, (const uint8_t*)msg, strlen(msg));
        ESP_LOGI(TAG, "UART0 init OK");
    }

    // ============================
    // 启动 ADC（★ 新的函数）
    // ============================
    ret = adc_tool_start(
            ADC_UNIT_1,
            ADC_CHANNEL_3,
            ADC_ATTEN_DB_12,
            adc_value_callback   // ★ 用户回调函数
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start ADC tool: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "ADC tool started successfully");

    // main 不再做任何 ADC 工作，因为 adc_tool_start 内部有任务
}