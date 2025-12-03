idf_component_register(
        SRCS "wasterwater-mcu.c" "modbus/rtu.c" "adc/adc.c" "gpio/gpio.c" "gpio/pwm.c" "uart/uart.c"
        INCLUDE_DIRS "." "modbus" "adc" "gpio" "uart"
        PRIV_REQUIRES esp_adc esp_driver_uart esp_driver_gpio
)
