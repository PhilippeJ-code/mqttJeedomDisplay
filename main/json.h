// json.h
//
#pragma once

extern void jsonInit();
extern void jsonSubscribe();
extern void jsonUpdate();
extern void jsonDataEvent(esp_mqtt_event_handle_t event);

extern void jsonParametersMqtt(esp_mqtt_client_config_t* mqtt_cfg);

extern void jsonMqttConnected();
extern void jsonMqttDisconnected();
