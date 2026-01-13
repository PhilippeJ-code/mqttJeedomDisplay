// WaveShare.c
//
#include "WaveShare.h"

void WaveShareInit()
{
#ifdef CONFIG_WS_USE_USB
    usbInit();
#endif
#if defined(CONFIG_WS_USE_USB) || defined(CONFIG_WS_USE_LVGL)
    i2cInit();
#ifdef CONFIG_WS_USE_LVGL
    gt911Init();
#endif
    ioExpanderInit();
#endif
#ifdef CONFIG_WS_USE_SDCARD
    sdCardInit();
#endif
#ifdef CONFIG_WS_USE_LVGL
    rgbPanelInit();
    hardLvglInit();
#endif
#ifdef CONFIG_WS_USE_NVS
    //nvsInit();
#endif

}