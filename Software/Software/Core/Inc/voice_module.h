#ifndef __VOICE_MODULE_H__
#define __VOICE_MODULE_H__

#include "main.h"
#include "sensors.h"
#include "esp8266.h"

void VoiceModule_Init(void);
void VoiceModule_RxCpltCallback(void);
void VoiceModule_Process(const SensorData_t *data, const Esp8266Info_t *wifi_info);
uint8_t VoiceModule_BroadcastId(uint8_t voice_id);

#endif
