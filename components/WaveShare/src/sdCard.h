// sdCard.h
//
#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_SDCARD

#define MOUNT_POINT "/sdcard"

void sdCardInit();

char sdCardMount();
char sdCardUnmount();
char sdCardPresent();

#endif