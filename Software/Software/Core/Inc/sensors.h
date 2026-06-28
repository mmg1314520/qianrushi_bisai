#ifndef __SENSORS_H__
#define __SENSORS_H__

#include "main.h"

typedef struct {
    int16_t  temperature;   /* ×10，例：255 = 25.5°C */
    int16_t  humidity;      /* ×10，例：605 = 60.5% */
    uint16_t co2_ppm;
    uint16_t light_adc;
    uint16_t soil_adc;
    uint8_t  dht11_ok;
    uint8_t  dht11_status;  /* 调试：0=正常 1~6=失败阶段 */
    uint8_t  co2_ok;
} SensorData_t;

void Sensors_Init(void);
void Sensors_ReadAll(SensorData_t *data);

#endif
