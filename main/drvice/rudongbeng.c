//
// Created by nebula on 2026/1/14.
//

#include "rudongbeng.h"

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "rtu.h"
#include "gpio/pwm.h"

static const char* TAG = "RUDONGBENG";

// ============================
// 蠕动泵PWM控制
// ============================
#define RUDONGBENG_PWM_GPIO_NUM           10  // 使用GPIO 8控制蠕动泵
#define RUDONGBENG_PWM_FREQUENCY          20000  // 20kHz PWM频率
#define RUDONGBENG_PWM_DUTY_RESOLUTION    LEDC_TIMER_10_BIT  // 10位分辨率 (0-1023)
#define RUDONGBENG_PWM_MAX_DUTY           1023   // 最大占空比值 (2^10 - 1)

static pwm_config_t rudongbeng_pwm_config = {
    .timer_num = LEDC_TIMER_1,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .frequency = RUDONGBENG_PWM_FREQUENCY,
    .duty_resolution = RUDONGBENG_PWM_DUTY_RESOLUTION,
    .channel = LEDC_CHANNEL_1,
    .gpio_num = RUDONGBENG_PWM_GPIO_NUM,
    .duty = 0, // 初始占空比为0
};

/**
 * @brief 设置蠕动泵速度
 *
 * 使用DRV8833电机驱动器，通过PWM信号控制蠕动泵速度
 * 接收1-100的百分比参数，并转换为对应的PWM占空比值
 */
void set_rudongbeng_sudo(int per)
{
    // 限制输入范围在1-100之间
    if (per < 1)
    {
        per = 1;
    }
    else if (per > 100)
    {
        per = 100;
    }

    // 计算对应百分比的PWM占空比值 (基于10位分辨率，最大值为1023)
    uint32_t calculated_duty = (uint32_t)((per / 100.0) * RUDONGBENG_PWM_MAX_DUTY);

    // 初始化PWM配置
    esp_err_t ret = pwm_init(&rudongbeng_pwm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize PWM for peristaltic pump: %s", esp_err_to_name(ret));
        return;
    }
    else
    {
        ESP_LOGI(TAG, "Peristaltic pump PWM initialized successfully on GPIO%d with %d Hz frequency",
                 RUDONGBENG_PWM_GPIO_NUM, RUDONGBENG_PWM_FREQUENCY);
    }

    // 设置PWM占空比
    ret = pwm_set_duty(rudongbeng_pwm_config.channel, calculated_duty, rudongbeng_pwm_config.speed_mode);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set peristaltic pump PWM duty: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Set peristaltic pump speed to %d%%, calculated duty value: %d", per, calculated_duty);

    state.rudongbeng_var = per;
}
