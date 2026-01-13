// i2c.c
//
#include "sdkconfig.h"

#if defined(CONFIG_WS_USE_USB) || defined(CONFIG_WS_USE_LVGL)

#include "i2c.h"

esp_err_t i2cInit()
{

    const i2c_port_t i2c_master_port = I2C_NUM_0;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master =
            {
                .clk_speed = FREQ_HZ,
            },
        .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL};

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);

}

#endif