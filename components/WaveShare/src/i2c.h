// i2c.h
//
#pragma once

#include "sdkconfig.h"

#if defined(CONFIG_WS_USE_USB) || defined(CONFIG_WS_USE_LVGL)

#include "driver/i2c.h"
#include "driver/gpio.h"

#define PIN_SCL                         (gpio_num_t)GPIO_NUM_9
#define PIN_SDA                         (gpio_num_t)GPIO_NUM_8

#define FREQ_HZ                         (400000)

esp_err_t i2cInit();

#endif