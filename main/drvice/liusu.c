#include "liusu.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rtu.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define FLOW_SENSOR_GPIO    GPIO_NUM_6
#define YF_S201_FACTOR      7.5f

static const char *TAG = "liusu";

/* 脉冲计数 */
static volatile uint32_t pulse_count = 0;

/* 互斥量保护脉冲计数 */
static SemaphoreHandle_t pulse_mutex;

/* GPIO 中断回调 */
static void IRAM_ATTR flow_gpio_isr_handler(void* arg)
{
    // 上升沿 +1
    pulse_count++;
}

/* GPIO 初始化（中断模式） */
static void flow_gpio_init(void)
{
    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << FLOW_SENSOR_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,   // 强烈建议外部上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_POSEDGE,    // 上升沿触发
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));

    // 创建互斥量
    pulse_mutex = xSemaphoreCreateMutex();

    // 安装 GPIO 中断服务
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(FLOW_SENSOR_GPIO, flow_gpio_isr_handler, NULL));
}

void liusu_task(void *arg)
{
    uint32_t count_sec = 0;
    uint32_t total = 0;
    int64_t last_time = esp_timer_get_time();

    ESP_LOGI(TAG, "流量计任务启动（GPIO 中断方式）");

    while (1) {
        int64_t now = esp_timer_get_time();

        if (now - last_time >= 1000000) { // 每秒统计一次
            // 临界区保护
            count_sec = pulse_count;
            pulse_count = 0;

            total += count_sec;

            float flow_l_min = (float)count_sec / YF_S201_FACTOR;
            state.liusu_var = flow_l_min;

            ESP_LOGI(TAG,
                     "实时流量: %.2f L/min, 本秒脉冲: %d, 总脉冲: %u",
                     flow_l_min,
                     count_sec,
                     total);

            last_time = now;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void liusu_init(void)
{
    ESP_LOGI(TAG, "初始化流量计 GPIO（C3 兼容版）");
    flow_gpio_init();
    ESP_LOGI(TAG, "流量计初始化完成");
}
