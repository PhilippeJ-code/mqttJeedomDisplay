// hardLvgl.h
//
#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_LVGL

void hardLvglInit();
char hardLvglLock();
void hardLvglUnlock();

#endif