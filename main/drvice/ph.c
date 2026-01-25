//
// Created by nebula on 2026/1/14.
//

#include "ph.h"


//
// Created by nebula on 2026/1/14.
//

#include "tds.h"


#include <stdio.h>

#include "adc.h"
#include "esp_log.h"
#include "rtu.h"
#include "uart.h"

static const char* TAG = "CHONG_SHUA";

// ★ adc_tool_start 会在后台任务中不断调用这个函数
void adc_value_callback(int raw, int voltage_mv)
{
    float voltage = voltage_mv / 1000.0;
    state.ph_var = -11.37774f * voltage + 21.677f;
}

void get_ph_var()
{
    // ============================
    // 启动 ADC（★ 新的函数）
    // ============================
    esp_err_t ret = adc_tool_start(
        ADC_UNIT_1,
        ADC_CHANNEL_2,
        ADC_ATTEN_DB_12,
        adc_value_callback // ★ 用户回调函数
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start ADC tool: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "ADC tool started successfully");
}
