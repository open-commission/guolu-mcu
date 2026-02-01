#include "liusu.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "gpioutil.h"
#include "driver/pcnt.h"
#include "rtu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FLOW_SENSOR_GPIO    GPIO_NUM_6

#define PCNT_UNIT_USED     PCNT_UNIT_0
#define PCNT_CHANNEL_USED  PCNT_CHANNEL_0

// YF-S201 公式
#define YF_S201_FACTOR     7.5f

static const char* TAG = "liusu";

static uint32_t total_pulses = 0;

static void flow_pcnt_init(void)
{
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = 6,
        .ctrl_gpio_num  = PCNT_PIN_NOT_USED,

        .channel        = PCNT_CHANNEL_USED,
        .unit           = PCNT_UNIT_USED,

        // 上升沿 +1，下降沿不计
        .pos_mode       = PCNT_COUNT_INC,
        .neg_mode       = PCNT_COUNT_DIS,

        .lctrl_mode     = PCNT_MODE_KEEP,
        .hctrl_mode     = PCNT_MODE_KEEP,

        .counter_h_lim  = 32767,
        .counter_l_lim  = 0,
    };

    ESP_ERROR_CHECK(pcnt_unit_config(&pcnt_config));

    // 滤波：防止抖动（单位 APB 时钟周期）
    // 80MHz / 1024 ≈ 78kHz -> 足够流量计用
    pcnt_set_filter_value(PCNT_UNIT_USED, 1024);
    pcnt_filter_enable(PCNT_UNIT_USED);

    pcnt_counter_pause(PCNT_UNIT_USED);
    pcnt_counter_clear(PCNT_UNIT_USED);
    pcnt_counter_resume(PCNT_UNIT_USED);

    ESP_LOGI(TAG, "PCNT 初始化完成 (GPIO %d)", 6);
}

void liusu_task(void* arg)
{
    int16_t pcnt_count = 0;
    int64_t last_report_time = esp_timer_get_time();

    ESP_LOGI(TAG, "流量计 PCNT 任务启动...");

    while (1)
    {
        int64_t now = esp_timer_get_time();

        if (now - last_report_time >= 1000000)   // 1 秒
        {
            // 读取并清零 PCNT
            pcnt_get_counter_value(PCNT_UNIT_USED, &pcnt_count);
            pcnt_counter_clear(PCNT_UNIT_USED);

            total_pulses += pcnt_count;

            /**
             * pulses_in_sec == Hz
             * flow_l_min = Hz / 7.5
             */
            float flow_l_min = (float)pcnt_count / YF_S201_FACTOR;

            state.liusu_var = flow_l_min;

            ESP_LOGI(TAG,
                     "实时流量: %.2f L/min, 本秒脉冲: %d, 总脉冲: %ld",
                     flow_l_min,
                     pcnt_count,
                     total_pulses);

            last_report_time = now;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void liusu_init(void)
{
    // GPIO 只需要输入即可
    gpio_init_s(FLOW_SENSOR_GPIO, GPIO_MODE_INPUT);

    // ⚠️ 强烈建议外部 4.7k~10k 上拉到 3.3V
    gpio_pullup_en(FLOW_SENSOR_GPIO);
    gpio_pulldown_dis(FLOW_SENSOR_GPIO);

    flow_pcnt_init();

    ESP_LOGI(TAG, "流量计初始化完成 (PCNT 模式)");
}

void set_activity(int activity)
{
    gpio_set_level(GPIO_NUM_5, activity);
}
