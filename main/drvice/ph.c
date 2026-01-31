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


// ★ adc_tool_start 会在后台任务中不断调用这个函数
void ph_value_callback(int raw, int voltage_mv)
{
    float voltage = voltage_mv / 1000.0;
    state.ph_var = -11.37774f * voltage + 21.677f;
    ESP_LOGI("PH", "PH: %.2f V: %.2f  R: %d", state.ph_var, voltage, raw);
}