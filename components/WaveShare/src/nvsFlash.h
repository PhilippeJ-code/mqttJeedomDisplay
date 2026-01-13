// nvsFlash.h
// 
#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_NVS

#include "stdint.h"

void nvsInit();
uint16_t nvsRead(char* name);
void nvsWrite(char* name, uint16_t value);

#endif