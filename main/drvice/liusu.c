//
// Created by nebula on 2026/1/14.
//

#include "liusu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "global_vars.h"

static const char* TAG = "LIU_SU";

// 流量传感器GPIO引脚
#define LIU_SU_GPIO_PIN         9
#define LIU_SU_GPIO_INTR_MODE   GPIO_INTR_POSEDGE  // 上升沿触发中断

// 测量参数
#define MEASUREMENT_INTERVAL_MS 1000  // 测量间隔：1秒

// 全局变量
static volatile uint32_t pulse_count = 0;      // 脉冲计数
static volatile uint32_t current_flow_rate = 0; // 当前流量值 (单位时间内脉冲数)
static bool flow_sensor_initialized = false;    // 传感器初始化标志

// GPIO中断服务程序
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    pulse_count++; // 增加脉冲计数
}

// 流量测量任务
static void flow_measurement_task(void* arg)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t last_pulse_count = 0;
    
    while(1) {
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(MEASUREMENT_INTERVAL_MS));
        
        // 计算单位时间内的脉冲数
        uint32_t temp_pulse_count = pulse_count;
        current_flow_rate = temp_pulse_count - last_pulse_count;
        last_pulse_count = temp_pulse_count;
        
        // 更新全局变量
        liusu_var = current_flow_rate;
        
        ESP_LOGD(TAG, "Flow rate: %d pulses/sec, Total pulses: %d", current_flow_rate, temp_pulse_count);
    }
}

/**
 * @brief 设置流速（对于流量传感器，此函数可能不需要实际操作）
 * 
 * @param per 百分比值（保留接口兼容性）
 */
void set_liusu_sudu(int per)
{
    ESP_LOGI(TAG, "Setting flow sensor speed control to %d%%", per);
    
    // 如果需要，这里可以实现对流量传感器的某些控制
    // 目前主要是初始化功能
    if (!flow_sensor_initialized) {
        // 初始化GPIO
        esp_err_t ret = gpio_init(LIU_SU_GPIO_PIN, GPIO_MODE_INPUT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize flow sensor GPIO: %s", esp_err_to_name(ret));
            return;
        }

        // 配置GPIO中断
        gpio_config_t io_conf = {};
        io_conf.intr_type = LIU_SU_GPIO_INTR_MODE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << LIU_SU_GPIO_PIN);
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // 启用上拉电阻
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        
        ret = gpio_config(&io_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure flow sensor GPIO interrupt: %s", esp_err_to_name(ret));
            return;
        }

        // 安装GPIO中断服务例程
        ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { // ESP_ERR_INVALID_STATE表示ISR服务已安装
            ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
            return;
        }

        // 附加中断处理程序
        ret = gpio_isr_handler_add(LIU_SU_GPIO_PIN, gpio_isr_handler, (void*) LIU_SU_GPIO_PIN);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add GPIO ISR handler: %s", esp_err_to_name(ret));
            return;
        }
        
        // 启动流量测量任务
        xTaskCreate(flow_measurement_task, "flow_measurement_task", 2048, NULL, 10, NULL);
        
        flow_sensor_initialized = true;
        
        ESP_LOGI(TAG, "Flow sensor initialized successfully on GPIO%d", LIU_SU_GPIO_PIN);
    }
}

/**
 * @brief 获取流量传感器的当前值
 * 
 * @return 返回单位时间内的脉冲数
 */
uint32_t get_liusu_var()
{
    return current_flow_rate; // 返回单位时间内的脉冲数
}
