// rgbPanel.h
//
#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_LVGL

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

#define LCD_PIXEL_CLOCK_HZ          (16 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL       1
#define LCD_BK_LIGHT_OFF_LEVEL      !LCD_BK_LIGHT_ON_LEVEL
#define LCD_PIN_NUM_BK_LIGHT        -1
#define LCD_PIN_NUM_HSYNC           46
#define LCD_PIN_NUM_VSYNC           3
#define LCD_PIN_NUM_DE              5
#define LCD_PIN_NUM_PCLK            7
#define LCD_PIN_NUM_DATA0           14 // B0
#define LCD_PIN_NUM_DATA1           38 // B1
#define LCD_PIN_NUM_DATA2           18 // B2
#define LCD_PIN_NUM_DATA3           17 // B3
#define LCD_PIN_NUM_DATA4           10 // B4
#define LCD_PIN_NUM_DATA5           39 // G0
#define LCD_PIN_NUM_DATA6           0  // G1
#define LCD_PIN_NUM_DATA7           45 // G2
#define LCD_PIN_NUM_DATA8           48 // G3
#define LCD_PIN_NUM_DATA9           47 // G4
#define LCD_PIN_NUM_DATA10          21 // G5
#define LCD_PIN_NUM_DATA11          1  // R0
#define LCD_PIN_NUM_DATA12          2  // R1
#define LCD_PIN_NUM_DATA13          42 // R2
#define LCD_PIN_NUM_DATA14          41 // R3
#define LCD_PIN_NUM_DATA15          40 // R4
#define LCD_PIN_NUM_DISP_EN         -1

extern esp_lcd_panel_handle_t   panel_handle;

void rgbPanelInit();

#endif