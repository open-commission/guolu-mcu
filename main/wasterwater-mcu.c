/*
 * SPDX-FileCopyrightText: 2016-2023 乐鑫信息科技(上海)有限公司
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// FreeModbus 从机示例 ESP32

#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include "mbcontroller.h"       // 用于 mbcontroller 定义和 API
#include "modbus_params.h"      // 用于 modbus 参数结构
#include "esp_log.h"            // 用于日志写入
#include "sdkconfig.h"

#define MB_PORT_NUM     (CONFIG_MB_UART_PORT_NUM)   // 用于 Modbus 连接的 UART 端口号
#define MB_SLAVE_ADDR   (CONFIG_MB_SLAVE_ADDR)      // Modbus 网络中设备的地址
#define MB_DEV_SPEED    (CONFIG_MB_UART_BAUD_RATE)  // UART 的通信速率

// 注意：目标芯片上的一些引脚不能分配给 UART 通信。
// 请参考所选板子和目标的文档，使用 Kconfig 配置引脚。

// 下面的定义用于为每种类型的 Modbus 寄存器定义起始地址
#define HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_t, field) >> 1))
#define INPUT_OFFSET(field) ((uint16_t)(offsetof(input_reg_params_t, field) >> 1))
#define MB_REG_DISCRETE_INPUT_START         (0x0000)
#define MB_REG_COILS_START                  (0x0000)
#define MB_REG_INPUT_START_AREA0            (INPUT_OFFSET(input_data0)) // 输入区域 0 的寄存器偏移量
#define MB_REG_INPUT_START_AREA1            (INPUT_OFFSET(input_data4)) // 输入区域 1 的寄存器偏移量
#define MB_REG_HOLDING_START_AREA0          (HOLD_OFFSET(holding_data0))
#define MB_REG_HOLDING_START_AREA1          (HOLD_OFFSET(holding_data4))

#define MB_PAR_INFO_GET_TOUT                (10) // 获取参数信息的超时时间
#define MB_CHAN_DATA_MAX_VAL                (6)
#define MB_CHAN_DATA_OFFSET                 (0.2f)
#define MB_READ_MASK                        (MB_EVENT_INPUT_REG_RD \
                                                | MB_EVENT_HOLDING_REG_RD \
                                                | MB_EVENT_DISCRETE_RD \
                                                | MB_EVENT_COILS_RD)
#define MB_WRITE_MASK                       (MB_EVENT_HOLDING_REG_WR \
                                                | MB_EVENT_COILS_WR)
#define MB_READ_WRITE_MASK                  (MB_READ_MASK | MB_WRITE_MASK)

static const char *TAG = "SLAVE_TEST";

static portMUX_TYPE param_lock = portMUX_INITIALIZER_UNLOCKED;

// 将寄存器值设置为已知状态
static void setup_reg_data(void)
{
    // 定义参数的初始状态
    discrete_reg_params.discrete_input0 = 1;
    discrete_reg_params.discrete_input1 = 0;
    discrete_reg_params.discrete_input2 = 1;
    discrete_reg_params.discrete_input3 = 0;
    discrete_reg_params.discrete_input4 = 1;
    discrete_reg_params.discrete_input5 = 0;
    discrete_reg_params.discrete_input6 = 1;
    discrete_reg_params.discrete_input7 = 0;

    holding_reg_params.holding_data0 = 1.34;
    holding_reg_params.holding_data1 = 2.56;
    holding_reg_params.holding_data2 = 3.78;
    holding_reg_params.holding_data3 = 4.90;

    holding_reg_params.holding_data4 = 5.67;
    holding_reg_params.holding_data5 = 6.78;
    holding_reg_params.holding_data6 = 7.79;
    holding_reg_params.holding_data7 = 8.80;

    coil_reg_params.coils_port0 = 0x55;
    coil_reg_params.coils_port1 = 0xAA;

    input_reg_params.input_data0 = 1.12;
    input_reg_params.input_data1 = 2.34;
    input_reg_params.input_data2 = 3.56;
    input_reg_params.input_data3 = 4.78;

    input_reg_params.input_data4 = 1.12;
    input_reg_params.input_data5 = 2.34;
    input_reg_params.input_data6 = 3.56;
    input_reg_params.input_data7 = 4.78;
}

// Modbus 从机的应用示例。它基于 freemodbus 协议栈。
// 有关分配的 Modbus 参数的更多信息，请参见 deviceparams.h 文件。
// 这些参数可以从主应用程序访问，也可以由外部 Modbus 主机更改。
void app_main(void)
{
    mb_param_info_t reg_info; // 保存 Modbus 寄存器访问信息
    mb_communication_info_t comm_info; // Modbus 通信参数
    mb_register_area_descriptor_t reg_area; // Modbus 寄存器区域描述符结构

    // 设置 UART 日志级别
    esp_log_level_set(TAG, ESP_LOG_INFO);
    void* mbc_slave_handler = NULL;

    ESP_ERROR_CHECK(mbc_slave_init(MB_PORT_SERIAL_SLAVE, &mbc_slave_handler)); // 初始化 Modbus 控制器

    // 设置通信模式并启动协议栈
#if CONFIG_MB_COMM_MODE_ASCII
    comm_info.mode = MB_MODE_ASCII;
#elif CONFIG_MB_COMM_MODE_RTU
    comm_info.mode = MB_MODE_RTU;
#endif
    comm_info.slave_addr = MB_SLAVE_ADDR;
    comm_info.port = MB_PORT_NUM;
    comm_info.baudrate = MB_DEV_SPEED;
    comm_info.parity = MB_PARITY_NONE;
    ESP_ERROR_CHECK(mbc_slave_setup((void*)&comm_info));

    // 下面的代码初始化 Modbus 寄存器区域描述符
    // 包括 Modbus 保持寄存器、输入寄存器、线圈和离散输入
    // 应根据寄存器映射为每个支持的 Modbus 寄存器区域进行初始化。
    // 当外部主机尝试访问未通过 mbc_slave_set_descriptor() API 调用初始化的区域中的寄存器时
    // Modbus 协议栈将为此寄存器区域发送异常响应。
    reg_area.type = MB_PARAM_HOLDING; // 设置寄存器区域类型
    reg_area.start_offset = MB_REG_HOLDING_START_AREA0; // Modbus 协议中寄存器区域的偏移量
    reg_area.address = (void*)&holding_reg_params.holding_data0; // 设置存储实例的指针
    // 设置寄存器存储实例的大小 = 150 个保持寄存器
    reg_area.size = (size_t)(HOLD_OFFSET(holding_data4) - HOLD_OFFSET(test_regs));
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));
    reg_area.type = MB_PARAM_HOLDING; // 设置寄存器区域类型
    reg_area.start_offset = MB_REG_HOLDING_START_AREA1; // Modbus 协议中寄存器区域的偏移量
    reg_area.address = (void*)&holding_reg_params.holding_data4; // 设置存储实例的指针
    reg_area.size = sizeof(float) << 2; // 设置寄存器存储实例的大小
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    // 初始化输入寄存器区域
    reg_area.type = MB_PARAM_INPUT;
    reg_area.start_offset = MB_REG_INPUT_START_AREA0;
    reg_area.address = (void*)&input_reg_params.input_data0;
    reg_area.size = sizeof(float) << 2;
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));
    reg_area.type = MB_PARAM_INPUT;
    reg_area.start_offset = MB_REG_INPUT_START_AREA1;
    reg_area.address = (void*)&input_reg_params.input_data4;
    reg_area.size = sizeof(float) << 2;
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    // 初始化线圈寄存器区域
    reg_area.type = MB_PARAM_COIL;
    reg_area.start_offset = MB_REG_COILS_START;
    reg_area.address = (void*)&coil_reg_params;
    reg_area.size = sizeof(coil_reg_params);
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    // 初始化离散输入寄存器区域
    reg_area.type = MB_PARAM_DISCRETE;
    reg_area.start_offset = MB_REG_DISCRETE_INPUT_START;
    reg_area.address = (void*)&discrete_reg_params;
    reg_area.size = sizeof(discrete_reg_params);
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    setup_reg_data(); // 将值设置为已知状态

    // 启动 modbus 控制器和协议栈
    ESP_ERROR_CHECK(mbc_slave_start());

    // 设置 UART 引脚号
    ESP_ERROR_CHECK(uart_set_pin(MB_PORT_NUM, CONFIG_MB_UART_TXD,
                            CONFIG_MB_UART_RXD, CONFIG_MB_UART_RTS,
                            UART_PIN_NO_CHANGE));

    // 将 UART 驱动模式设置为半双工
    ESP_ERROR_CHECK(uart_set_mode(MB_PORT_NUM, UART_MODE_UART));

    ESP_LOGI(TAG, "Modbus slave stack initialized.");
    ESP_LOGI(TAG, "Start modbus test...");

    // 当参数 holdingRegParams.dataChan0 每次访问周期递增
    // 达到 CHAN_DATA_MAX_VAL 值时，下面的循环将终止。
    for(;holding_reg_params.holding_data0 < MB_CHAN_DATA_MAX_VAL;) {
        // 检查 Modbus 主机的读/写事件
        (void)mbc_slave_check_event(MB_READ_WRITE_MASK);
        ESP_ERROR_CHECK_WITHOUT_ABORT(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
        const char* rw_str = (reg_info.type & MB_READ_MASK) ? "READ" : "WRITE";
        // 过滤事件并相应地处理它们
        if(reg_info.type & (MB_EVENT_HOLDING_REG_WR | MB_EVENT_HOLDING_REG_RD)) {
            // 从参数队列获取参数信息
            ESP_LOGI(TAG, "HOLDING %s (%" PRIu32 " us), ADDR:%u, TYPE:%u, INST_ADDR:0x%" PRIx32 ", SIZE:%u",
                            rw_str,
                            reg_info.time_stamp,
                            (unsigned)reg_info.mb_offset,
                            (unsigned)reg_info.type,
                            (uint32_t)reg_info.address,
                            (unsigned)reg_info.size);
            if (reg_info.address == (uint8_t*)&holding_reg_params.holding_data0)
            {
                portENTER_CRITICAL(&param_lock);
                holding_reg_params.holding_data0 += MB_CHAN_DATA_OFFSET;
                if (holding_reg_params.holding_data0 >= (MB_CHAN_DATA_MAX_VAL - MB_CHAN_DATA_OFFSET)) {
                    coil_reg_params.coils_port1 = 0xFF;
                }
                portEXIT_CRITICAL(&param_lock);
            }
        } else if (reg_info.type & MB_EVENT_INPUT_REG_RD) {
            ESP_LOGI(TAG, "INPUT READ (%" PRIu32 " us), ADDR:%u, TYPE:%u, INST_ADDR:0x%" PRIx32 ", SIZE:%u",
                            reg_info.time_stamp,
                            (unsigned)reg_info.mb_offset,
                            (unsigned)reg_info.type,
                            (uint32_t)reg_info.address,
                            (unsigned)reg_info.size);
        } else if (reg_info.type & MB_EVENT_DISCRETE_RD) {
            ESP_LOGI(TAG, "DISCRETE READ (%" PRIu32 " us): ADDR:%u, TYPE:%u, INST_ADDR:0x%" PRIx32 ", SIZE:%u",
                            reg_info.time_stamp,
                            (unsigned)reg_info.mb_offset,
                            (unsigned)reg_info.type,
                            (uint32_t)reg_info.address,
                            (unsigned)reg_info.size);
        } else if (reg_info.type & (MB_EVENT_COILS_RD | MB_EVENT_COILS_WR)) {
            ESP_LOGI(TAG, "COILS %s (%" PRIu32 " us), ADDR:%u, TYPE:%u, INST_ADDR:0x%" PRIx32 ", SIZE:%u",
                            rw_str,
                            reg_info.time_stamp,
                            (unsigned)reg_info.mb_offset,
                            (unsigned)reg_info.type,
                            (uint32_t)reg_info.address,
                            (unsigned)reg_info.size);
            if (coil_reg_params.coils_port1 == 0xFF) break;
        }
    }
    // 报警时销毁 Modbus 控制器
    ESP_LOGI(TAG,"Modbus controller destroyed.");
    vTaskDelay(100);
    ESP_ERROR_CHECK(mbc_slave_destroy());
}