//
// Created by nebula on 2026/1/31.
//

#include "water_task.h"

#include <stdio.h>

#include "adc.h"
#include "esp_log.h"
#include "ph.h"
#include "tds.h"
#include "uart.h"

void tds_task(void* arg)
{
    // ============================
    // 启动 ADC（★ 新的函数）
    // ============================
    esp_err_t ret = adc_tool_start(
        ADC_UNIT_1,
        ADC_CHANNEL_3,
        ADC_ATTEN_DB_12,
        tds_value_callback // ★ 用户回调函数
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE("adc", "Failed to start ADC tool: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI("adc", "ADC tool started successfully");

    while (1)
    {
        vTaskDelay(1000);
    }
}

void ph_task(void* arg)
{
    // ============================
    // 启动 ADC（★ 新的函数）
    // ============================
    esp_err_t ret = adc_tool_start(
        ADC_UNIT_1,
        ADC_CHANNEL_2,
        ADC_ATTEN_DB_12,
        ph_value_callback // ★ 用户回调函数
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE("adc", "Failed to start ADC tool: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI("adc", "ADC tool started successfully");

    while (1)
    {
        vTaskDelay(1000);
    }
}
