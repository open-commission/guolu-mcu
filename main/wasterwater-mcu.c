#include "adc/adc.h"
#include "uart/uart.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "gpio/pwm.h"

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
// PWM控制任务
// ============================
#define PWM_GPIO_NUM           7
#define PWM_FREQUENCY          20000  // 20kHz
#define PWM_DUTY_RESOLUTION    LEDC_TIMER_10_BIT  // 10位分辨率 (0-1023)
#define PWM_MAX_DUTY           1023   // 最大占空比值 (2^10 - 1)
#define PWM_MIN_DUTY           700    // 20%占空比 (1023 * 0.2)
#define PWM_MID_DUTY           800    // 80%占空比 (1023 * 0.8)
#define PWM_CYCLE_PERIOD_MS    1000   // 1秒周期

static pwm_config_t pwm_config = {
    .timer_num = LEDC_TIMER_0,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .frequency = PWM_FREQUENCY,
    .duty_resolution = PWM_DUTY_RESOLUTION,
    .channel = LEDC_CHANNEL_0,
    .gpio_num = PWM_GPIO_NUM,
    .duty = PWM_MIN_DUTY,  // 初始占空比为20%
};

/**
 * @brief PWM控制任务
 * 
 * 控制PWM占空比在20%-80%-20%之间循环变化，周期为1秒
 */
void pwm_control_task(void)
{
    static uint8_t state = 0; // 0: 20%, 1: 80%, 2: 20%
    
    switch (state) {
        case 0: // 保持20%占空比500ms
            ESP_LOGD("PWM_TASK", "Setting PWM duty to 20%%");
            pwm_set_duty(pwm_config.channel, PWM_MIN_DUTY, pwm_config.speed_mode);
            state = 1;
            break;
            
        case 1: // 改变到80%占空比并保持500ms
            ESP_LOGD("PWM_TASK", "Setting PWM duty to 80%%");
            pwm_set_duty(pwm_config.channel, PWM_MID_DUTY, pwm_config.speed_mode);
            state = 2;
            break;
            
        case 2: // 改变回20%占空比
            ESP_LOGD("PWM_TASK", "Setting PWM duty back to 20%%");
            pwm_set_duty(pwm_config.channel, PWM_MIN_DUTY, pwm_config.speed_mode);
            state = 0;
            break;
            
        default:
            state = 0;
            break;
    }
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

    // ============================
    // 初始化 PWM 输出
    // ============================
    ret = pwm_init(&pwm_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PWM: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "PWM initialized successfully on GPIO%d with %d Hz frequency", 
                 PWM_GPIO_NUM, PWM_FREQUENCY);
    }

    // ============================
    // 主循环 - 控制 PWM 占空比变化
    // ============================
    // 占空比变化周期: 20% -> 80% -> 20%，每1秒切换一次状态
    static uint32_t last_pwm_update = 0;
    while (1) {
        uint32_t current_time = xTaskGetTickCount();
        
        // 每隔一定时间更新PWM占空比
        if ((current_time - last_pwm_update) >= pdMS_TO_TICKS(500)) {
            pwm_control_task();
            last_pwm_update = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // 小延时以避免过度占用CPU
    }
}