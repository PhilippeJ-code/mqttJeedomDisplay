// ioExpander.h
//
#pragma once

#include "sdkconfig.h"

#if defined(CONFIG_WS_USE_USB) || defined(CONFIG_WS_USE_LVGL)

#ifdef __cplusplus
extern "C" {
#endif

void ioExpanderInit();
void turn_backlight_off();
void turn_backlight_on();

void reset_panel();

#ifdef __cplusplus
} 
#endif

#endif