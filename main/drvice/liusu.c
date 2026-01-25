#include "liusu.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "gpio.h"
#include "rtu.h"
#include "freertos/FreeRTOS.h"

#define FLOW_SENSOR_GPIO    GPIO_NUM_6
// YF-S201 公式: F(Hz) = 7.5 * Q(L/min) -> Q = F / 7.5
#define YF_S201_FACTOR      7.5f

static const char* TAG = "liusu";
static uint32_t total_pulses = 0;

void liusu_task(void* arg)
{
    int last_level = 0;
    uint32_t pulses_in_sec = 0;
    int64_t last_report_time = esp_timer_get_time();

    ESP_LOGI(TAG, "流量计轮询任务启动...");

    while (1)
    {
        // 1. 读取当前电平
        int current_level = gpio_get_level(FLOW_SENSOR_GPIO);

        // 2. 检测上升沿 (从 0 变 1)
        if (last_level == 0 && current_level == 1)
        {
            pulses_in_sec++;
            total_pulses++;
        }
        last_level = current_level;

        // 3. 每隔 1 秒进行换算
        int64_t now = esp_timer_get_time();
        if (now - last_report_time >= 1000000)
        {
            /**
             * 换算逻辑：
             * pulses_in_sec 实际上就是频率 Hz (每秒脉冲数)
             * flow_l_min = Hz / 7.5 (单位: L/min)
             * 如果需要换算成 升/小时 (L/h)，则再乘以 60
             */
            float flow_l_min = (float)pulses_in_sec / YF_S201_FACTOR;

            // --- 存入 state 结构体 ---
            // 确保 state.liusu_var 是 float 类型，以便保留小数精度
            state.liusu_var = flow_l_min;

            // 如果你还需要累计总量（单位：升），可以取消下面两行的注释：
            // static float total_liters = 0;
            // total_liters += (flow_l_min / 60.0f); // 每一秒累加这一秒流过的升数
            // state.total_flow = total_liters;

            ESP_LOGI(TAG, "实时流量: %.2f L/min, 总脉冲: %ld", flow_l_min, total_pulses);

            pulses_in_sec = 0;
            last_report_time = now;
        }

        // 1ms 轮询一次。注意：如果你的 FreeRTOS Tick 不是 1000Hz，
        // 这里可能会变成延时多个 ms。确保 menuconfig 中 Tick Rate 是 1000。
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void liusu_init(void)
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << FLOW_SENSOR_GPIO),
        .pull_up_en = 1, // 必须开启上拉，因为 YF-S201 内部通常是开漏输出
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE, // 轮询模式关闭中断
    };
    gpio_config(&io_conf);

    // 建议增加一个简单的 log 确认初始化成功
    ESP_LOGI(TAG, "GPIO %d 初始化成功 (轮询模式)", FLOW_SENSOR_GPIO);

    xTaskCreate(liusu_polling_task, "liusu_task", 4096, NULL, 10, NULL);
}

void set_activity(int activity)
{
    gpio_set_level(GPIO_NUM_5, activity);
}
