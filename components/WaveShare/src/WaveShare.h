// WaveShare.h
//
#pragma once

#include "sdkconfig.h"

#if defined(CONFIG_WS_USE_USB) || defined(CONFIG_WS_USE_LVGL)
#include "i2c.h"
#include "ioExpander.h"
#include "usb.h"
#endif

#ifdef CONFIG_WS_USE_SDCARD
#include "sdCard.h"
#endif

#ifdef CONFIG_WS_USE_LVGL
#include "gt911.h"
#include "rgbPanel.h"
#include "hardLvgl.h"
#endif

#ifdef CONFIG_WS_USE_NVS
#include "nvsFlash.h"
#endif

void WaveShareInit();
