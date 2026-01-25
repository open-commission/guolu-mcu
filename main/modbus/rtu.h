//
// Created by nebula on 2025/12/3.
//

#ifndef WASTERWATER_MCU_RTU_H
#define WASTERWATER_MCU_RTU_H

void set_reg_state();

void modbus_task(void* arg);

typedef struct
{
    float liusu_var;
    float tds_var;
    float ph_var;
    float rudongbeng_var;
    float chongshua_var;
    int activity_state;
} state_t;

extern state_t state;

#endif //WASTERWATER_MCU_RTU_H
