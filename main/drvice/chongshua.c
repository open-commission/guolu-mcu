//
// Created by nebula on 2026/1/14.
//

#include "chongshua.h"
#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "global_vars.h"
#include "freertos/projdefs.h"
#include "gpio/pwm.h"

static const char* TAG = "CHONG_SHUA";

// ============================
// PWM控制任务
// ============================
#define PWM_GPIO_NUM           7
#define PWM_FREQUENCY          20000  // 20kHz
#define PWM_DUTY_RESOLUTION    LEDC_TIMER_10_BIT  // 10位分辨率 (0-1023)
#define PWM_MAX_DUTY           1023   // 最大占空比值 (2^10 - 1)

static pwm_config_t pwm_config = {
    .timer_num = LEDC_TIMER_0,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .frequency = PWM_FREQUENCY,
    .duty_resolution = PWM_DUTY_RESOLUTION,
    .channel = LEDC_CHANNEL_0,
    .gpio_num = PWM_GPIO_NUM,
    .duty = 0, // 初始占空比为0
};

/**
 * @brief 设置冲刷速度
 *
 * 使用DRV8833电机驱动器，通过PWM信号控制电机速度
 * 接收1-100的百分比参数，并转换为对应的PWM占空比值
 */
void set_chongshua_sudu(int per)
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
    uint32_t calculated_duty = (uint32_t)((per / 100.0) * PWM_MAX_DUTY);

    // 初始化PWM配置
    esp_err_t ret = pwm_init(&pwm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize PWM: %s", esp_err_to_name(ret));
        return;
    }
    else
    {
        ESP_LOGI(TAG, "PWM initialized successfully on GPIO%d with %d Hz frequency",
                 PWM_GPIO_NUM, PWM_FREQUENCY);
    }

    // 设置PWM占空比
    ret = pwm_set_duty(pwm_config.channel, calculated_duty, pwm_config.speed_mode);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set PWM duty: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Set chongshua speed to %d%%, calculated duty value: %d", per, calculated_duty);

    chonsghua_val = per;
}
