// usb.h
//
#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_WS_USE_USB

typedef void (*receive_event_t)(const uint8_t *data, size_t data_len);
typedef void (*connected_event_t)();
typedef void (*disconnected_event_t)();

#ifdef __cplusplus
extern "C" {
#endif

void usbInit();
void usbSetReveiveEvent(receive_event_t *re);
void usbSetConnectedEvent(connected_event_t *ce);
void usbSetDisconnectedEvent(disconnected_event_t *de);
void usbStart();

bool usbIsConnected();
void usbSend(uint8_t data[], size_t size);

#ifdef __cplusplus
} 
#endif

#endif