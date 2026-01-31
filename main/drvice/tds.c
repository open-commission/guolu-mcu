//
// Created by nebula on 2026/1/14.
//

#include "tds.h"


#include <stdio.h>

#include "adc.h"
#include "esp_log.h"
#include "rtu.h"
#include "uart.h"

// ★ adc_tool_start 会在后台任务中不断调用这个函数
void tds_value_callback(int raw, int voltage_mv)
{
    float voltage = voltage_mv / 1000.0;
    state.tds_var = 66.71 * voltage * voltage *
        voltage - 127.93 * voltage * voltage + 428.7 *
        voltage;

    ESP_LOGI("tds", "tds: %f  Voltage: %f mV raw: %d",
             state.tds_var, voltage, raw);
}
