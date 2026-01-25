//
// Created by nebula on 2026/1/14.
//

#include "chongshua.h"
#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "rtu.h"
#include "gpio/pwm.h"

static const char* TAG = "CHONG_SHUA";

// ============================
// PWM 配置
// ============================
#define PWM_GPIO_NUM           11
#define PWM_FREQUENCY          20000                 // 20kHz
#define PWM_DUTY_RESOLUTION    LEDC_TIMER_10_BIT      // 10位分辨率
#define PWM_MAX_DUTY           1023

// 电源与电机参数
#define MOTOR_RATED_VOLTAGE    3.3f                  // 电机额定电压
#define SUPPLY_VOLTAGE         12.0f                 // 系统供电电压

// 最大允许 PWM 占空比（防止过压）
#define MOTOR_MAX_DUTY_RATIO   (MOTOR_RATED_VOLTAGE / SUPPLY_VOLTAGE)
#define MOTOR_MAX_DUTY         ((uint32_t)(PWM_MAX_DUTY * MOTOR_MAX_DUTY_RATIO))

static pwm_config_t pwm_config = {
    .timer_num = LEDC_TIMER_0,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .frequency = PWM_FREQUENCY,
    .duty_resolution = PWM_DUTY_RESOLUTION,
    .channel = LEDC_CHANNEL_0,
    .gpio_num = PWM_GPIO_NUM,
    .duty = 0,
};

/**
 * @brief 设置冲刷电机速度
 *
 * @param per 0~100，表示百分比速度
 *
 * 说明：
 * - 0%  → 电机停止
 * - 100% → 等效 3.3V（12V * 27.5%）
 * - 自动限制最大 PWM，占空比永不超过电机额定电压
 */
void set_chongshua_sudu(int per)
{
    // ---------- 参数保护 ----------
    if (per < 0)
    {
        per = 0;
    }
    else if (per > 100)
    {
        per = 100;
    }

    // ---------- 百分比 → PWM ----------
    uint32_t duty = (uint32_t)((per / 100.0f) * MOTOR_MAX_DUTY);

    // ---------- 初始化 PWM（只需一次，重复调用也安全） ----------
    static bool pwm_inited = false;
    if (!pwm_inited)
    {
        esp_err_t ret = pwm_init(&pwm_config);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "PWM init failed: %s", esp_err_to_name(ret));
            return;
        }

        pwm_inited = true;
        ESP_LOGI(TAG,
                 "PWM initialized: GPIO=%d, freq=%dHz, max_duty=%lu (%.1f%%)",
                 PWM_GPIO_NUM,
                 PWM_FREQUENCY,
                 MOTOR_MAX_DUTY,
                 MOTOR_MAX_DUTY_RATIO * 100.0f);
    }

    // ---------- 设置占空比 ----------
    esp_err_t ret = pwm_set_duty(pwm_config.channel, duty, pwm_config.speed_mode);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Set PWM duty failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG,
             "Chongshua speed=%d%%, duty=%lu / %d (%.2fV eq)",
             per,
             duty,
             PWM_MAX_DUTY,
             SUPPLY_VOLTAGE * ((float)duty / PWM_MAX_DUTY));

    // ---------- 状态记录 ----------
    state.chongshua_var = per;
}
