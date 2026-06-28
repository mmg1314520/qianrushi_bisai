#ifndef __DISPLAY_UI_H__
#define __DISPLAY_UI_H__

#include "sensors.h"
#include "esp8266.h"

void DisplayUI_Init(void);
void DisplayUI_ShowStatus(const char *status);
void DisplayUI_Update(const SensorData_t *data, const Esp8266Info_t *info);
void DisplayUI_Process(void);

#endif /* __DISPLAY_UI_H__ */
