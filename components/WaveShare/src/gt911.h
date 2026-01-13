// gt911.h
//
#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_LVGL

#include "esp_lcd_touch_gt911.h"

#define LCD_H_RES                       800
#define LCD_V_RES                       480

#define TOUCH_PIN_RESET                 (gpio_num_t)GPIO_NUM_NC
#define TOUCH_PIN_INT                   (gpio_num_t)GPIO_NUM_NC

void gt911Init();
esp_lcd_touch_handle_t gt911GetTp();

#endif