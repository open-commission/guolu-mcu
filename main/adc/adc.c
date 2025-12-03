//
// Created by nebula on 2025/12/3.
//

#include "adc.h"

/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_continuous.h"

// ADC单元配置
#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
// 宏定义辅助函数，用于将ADC单元转换为字符串
#define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
#define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)
// ADC转换模式：单个单元转换
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
// ADC衰减设置：12dB衰减
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_12
// ADC位宽设置：使用SOC支持的最大位宽
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

// 根据芯片类型选择不同的ADC输出格式和数据获取方式
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
// ESP32/ESP32S2 使用TYPE1输出格式
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
// 获取通道号的宏定义（TYPE1）
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
// 获取ADC数据的宏定义（TYPE1）
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
// 其他芯片使用TYPE2输出格式
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
// 获取通道号的宏定义（TYPE2）
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
// 获取ADC数据的宏定义（TYPE2）
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif

// 每次读取的数据长度（字节）
#define EXAMPLE_READ_LEN                    256

// 根据目标芯片配置ADC通道
#if CONFIG_IDF_TARGET_ESP32
// ESP32使用两个通道：ADC_CHANNEL_6 和 ADC_CHANNEL_7
static adc_channel_t channel[2] = {ADC_CHANNEL_6, ADC_CHANNEL_7};
#else
// 其他芯片使用一个通道：ADC_CHANNEL_2
static adc_channel_t channel[1] = {ADC_CHANNEL_2};
#endif

// 任务句柄，用于通知ADC转换完成
static TaskHandle_t s_task_handle;
// 日志标签
static const char* TAG = "EXAMPLE";

/**
 * @brief ADC连续转换完成回调函数
 * 
 * @param handle ADC连续驱动句柄
 * @param edata 事件数据指针
 * @param user_data 用户数据指针
 * @return true 需要任务切换
 * @return false 不需要任务切换
 */
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t* edata,
                                     void* user_data)
{
    BaseType_t mustYield = pdFALSE;
    // 通知任务ADC连续驱动已完成足够的转换次数
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

/**
 * @brief 初始化连续ADC
 * 
 * @param channel ADC通道数组
 * @param channel_num 通道数量
 * @param out_handle 输出的ADC句柄
 */
static void continuous_adc_init(adc_channel_t* channel, uint8_t channel_num, adc_continuous_handle_t* out_handle)
{
    adc_continuous_handle_t handle = NULL;

    // ADC连续模式句柄配置
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,      // 最大存储缓冲区大小
        .conv_frame_size = EXAMPLE_READ_LEN,  // 转换帧大小
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    // ADC连续模式配置
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,     // 采样频率：20KHz
        .conv_mode = EXAMPLE_ADC_CONV_MODE,  // 转换模式
        .format = EXAMPLE_ADC_OUTPUT_TYPE,   // 输出格式
    };

    // ADC模式配置数组
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++)
    {
        // 设置每个通道的参数
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;        // 衰减
        adc_pattern[i].channel = channel[i] & 0x7;       // 通道号
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;          // ADC单元
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH; // 位宽

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

/**
 * @brief 主函数入口
 * 
 */
void app_main(void)
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    // 存储ADC结果的缓冲区
    uint8_t result[EXAMPLE_READ_LEN] = {0};
    memset(result, 0xcc, EXAMPLE_READ_LEN);

    // 获取当前任务句柄
    s_task_handle = xTaskGetCurrentTaskHandle();

    // 初始化ADC连续模式
    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    // 注册ADC事件回调函数
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,  // 转换完成回调
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    // 启动ADC连续转换
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    while (1)
    {
        /**
         * 这里演示如何使用ADC连续模式驱动事件回调。
         * 当任务中数据处理速度快时，此`ulTaskNotifyTake`会阻塞。
         * 但在此示例中，数据处理（打印）较慢，因此几乎不会阻塞。
         *
         * 如果不使用此事件回调（通知此任务），仍然可以直接在此循环中调用
         * `adc_continuous_read()`，可以有或没有特定的阻塞超时。
         */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 获取ADC单元字符串表示
        char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);

        while (1)
        {
            // 读取ADC数据
            ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK)
            {
                ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, ret_num);
                // 处理返回的ADC数据
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
                {
                    adc_digi_output_data_t* p = (adc_digi_output_data_t*)&result[i];
                    uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);  // 获取通道号
                    uint32_t data = EXAMPLE_ADC_GET_DATA(p);         // 获取ADC值
                    /* 检查通道号有效性，如果通道号超过最大通道数，则数据无效 */
                    if (chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT))
                    {
                        ESP_LOGI(TAG, "Unit: %s, Channel: %"PRIu32", Value: %"PRIx32, unit, chan_num, data);
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Invalid data [%s_%"PRIu32"_%"PRIx32"]", unit, chan_num, data);
                    }
                }
                /**
                 * 因为打印速度较慢，所以每次调用`ulTaskNotifyTake`都会立即返回。
                 * 为了避免任务看门狗超时，在此处添加延迟。当你替换数据处理方式时，
                 * 通常不需要此延迟（因为此任务会阻塞一段时间）。
                 */
                vTaskDelay(1);
            }
            else if (ret == ESP_ERR_TIMEOUT)
            {
                // 尝试读取`EXAMPLE_READ_LEN`直到API返回超时，这意味着没有可用数据
                break;
            }
        }
    }

    // 停止并释放ADC资源
    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}
