#ifndef __BLUETOOTH_LINK_H__
#define __BLUETOOTH_LINK_H__

#include "main.h"
#include "sensors.h"
#include "esp8266.h"

void BluetoothLink_SendTelemetry(const SensorData_t *data, const Esp8266Info_t *wifi_info);
void BluetoothLink_ProcessLine(const char *line);
uint8_t BluetoothLink_IsAutoMode(void);
void BluetoothLink_SetAutoMode(uint8_t auto_mode);

#endif
