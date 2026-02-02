idf_component_register(
        SRCS "wasterwater-mcu.c" "modbus/rtu.c" "adc/adc.c"
        "gpio/gpioutil.c" "gpio/pwm.c" "uart/uart.c"
        "drvice/chongshua.c" "drvice/tds.c" "drvice/liusu.c"
        "drvice/ph.c" "drvice/rudongbeng.c" "drvice/tds.c"
        "task/water_task.c"
        INCLUDE_DIRS "." "modbus" "adc" "gpio" "uart" "drvice" "task"
        PRIV_REQUIRES esp_adc esp_driver_uart esp_driver_gpio driver
)

